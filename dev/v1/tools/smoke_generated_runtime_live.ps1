param(
	[string]$BaseUrl = 'http://127.0.0.1',
	[string]$RuntimeDir,
	[string]$PluginXid,
	[string]$GenerateXid,
	[string]$Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path,
	[string]$AdminBase = '/admin',
	[string]$AdminLoginPath = '',
	[string]$CookieHeader = '',
	[string]$AdminUsername = '',
	[string]$AdminPassword = '',
	[string]$AdminPasswordHash = '',
	[string]$ServerExe = '',
	[string]$ServerCwd = '',
	[int]$TimeoutSec = 10,
	[int]$StartupTimeoutSec = 20,
	[switch]$StartServer,
	[switch]$StopStartedServer,
	[switch]$EnableGeneratedPlugin,
	[switch]$RememberLogin,
	[switch]$SkipContentCheck
)

$ErrorActionPreference = 'Stop'

function New-SmokeWebSession($baseUrl) {
	$session = New-Object Microsoft.PowerShell.Commands.WebRequestSession
	if (![string]::IsNullOrWhiteSpace($CookieHeader)) {
		$uri = [uri]$baseUrl
		foreach ($part in ($CookieHeader -split ';')) {
			$item = $part.Trim()
			if ($item -match '^([^=]+)=(.*)$') {
				$session.Cookies.Add($uri, (New-Object System.Net.Cookie($Matches[1], $Matches[2], '/')))
			}
		}
	}
	return $session
}

function Update-SmokeCookieFromLogin($baseUrl, $timeoutSec) {
	$loginArgs = @(
		'-BaseUrl', $baseUrl,
		'-AdminBase', $AdminBase,
		'-TimeoutSec', $timeoutSec
	)
	if (![string]::IsNullOrWhiteSpace($AdminLoginPath)) { $loginArgs += @('-AdminLoginPath', $AdminLoginPath) }
	if (![string]::IsNullOrWhiteSpace($AdminUsername)) { $loginArgs += @('-Username', $AdminUsername) }
	if (![string]::IsNullOrWhiteSpace($AdminPassword)) { $loginArgs += @('-Password', $AdminPassword) }
	if (![string]::IsNullOrWhiteSpace($AdminPasswordHash)) { $loginArgs += @('-PasswordHash', $AdminPasswordHash) }
	if ($RememberLogin) { $loginArgs += '-Remember' }
	$script = Join-Path $PSScriptRoot 'get_admin_cookie.ps1'
	$newCookie = (& powershell -ExecutionPolicy Bypass -File $script @loginArgs | Select-Object -Last 1)
	if ([string]::IsNullOrWhiteSpace($newCookie)) {
		throw 'get_admin_cookie.ps1 did not return a CookieHeader after host reload'
	}
	$script:CookieHeader = $newCookie
	$script:SmokeWebSession = New-SmokeWebSession $baseUrl
}

function Invoke-HostReload($baseUrl, $timeoutSec) {
	$adminBase = '/' + $AdminBase.Trim('/')
	$url = $baseUrl.TrimEnd('/') + $adminBase + '/tool/reload/host'
	try {
		$response = Invoke-WebRequest -Uri $url -Method Post -TimeoutSec $timeoutSec -UseBasicParsing -ContentType 'application/json' -Body '{}' -WebSession $SmokeWebSession
		$json = [string]$response.Content | ConvertFrom-Json
		if ($true -ne [bool]$json.result) {
			throw "host reload failed: $($json.message)"
		}
		return $true
	} catch {
		throw "host reload request failed: $url $($_.Exception.Message)"
	}
}

function Test-LiveServer($baseUrl, $timeoutSec) {
	try {
		Invoke-WebRequest -Uri ($baseUrl.TrimEnd('/') + '/') -Method Get -TimeoutSec $timeoutSec -UseBasicParsing | Out-Null
		return $true
	} catch {
		if ($_.Exception.Response) {
			$status = [int]$_.Exception.Response.StatusCode
			return ($status -ge 200 -and $status -lt 500)
		}
		return $false
	}
}

function Wait-LiveServer($baseUrl, $timeoutSec, $startupTimeoutSec) {
	$deadline = (Get-Date).AddSeconds($startupTimeoutSec)
	while ((Get-Date) -lt $deadline) {
		if (Test-LiveServer $baseUrl $timeoutSec) {
			return $true
		}
		Start-Sleep -Milliseconds 500
	}
	return $false
}

function Invoke-ContentGenerate($baseUrl, $xid, $timeoutSec) {
	$adminBase = '/' + $AdminBase.Trim('/')
	$url = $baseUrl.TrimEnd('/') + $adminBase + '/content/generate?xid=' + [uri]::EscapeDataString($xid)
	try {
		$response = Invoke-WebRequest -Uri $url -Method Get -TimeoutSec $timeoutSec -UseBasicParsing -WebSession $SmokeWebSession
		$json = [string]$response.Content | ConvertFrom-Json
		if (($true -ne [bool]$json.result) -and ($true -ne [bool]$json.success)) {
			throw "content generate failed: $($json.message)"
		}
		$pluginXid = [string]$json.data.pluginXid
		if ([string]::IsNullOrWhiteSpace($pluginXid)) {
			throw 'content generate response missing data.pluginXid'
		}
		return $pluginXid
	} catch {
		throw "content generate request failed: $url $($_.Exception.Message)"
	}
}

function Invoke-PluginAction($baseUrl, $pluginXid, $action, $timeoutSec) {
	$adminBase = '/' + $AdminBase.Trim('/')
	$url = $baseUrl.TrimEnd('/') + $adminBase + '/plugin/' + $action
	try {
		$body = @{ name = $pluginXid } | ConvertTo-Json -Compress
		$response = Invoke-WebRequest -Uri $url -Method Post -TimeoutSec $timeoutSec -UseBasicParsing -ContentType 'application/json' -Body $body -WebSession $SmokeWebSession
		$json = [string]$response.Content | ConvertFrom-Json
		if ($true -ne [bool]$json.result) {
			throw "plugin $action failed: $($json.message)"
		}
		return $true
	} catch {
		throw "plugin $action request failed: $url $($_.Exception.Message)"
	}
}

if ([string]::IsNullOrWhiteSpace($ServerCwd)) {
	$ServerCwd = $Root
}
if ([string]::IsNullOrWhiteSpace($ServerExe)) {
	$ServerExe = Join-Path $Root 'xs.exe'
}

$started = $null
$SmokeWebSession = New-SmokeWebSession $BaseUrl
$alreadyLive = Test-LiveServer $BaseUrl $TimeoutSec
if (!$alreadyLive -and $StartServer) {
	if (!(Test-Path $ServerExe)) {
		throw "ServerExe not found: $ServerExe"
	}
	$logDir = Join-Path $Root 'data/logs'
	if (!(Test-Path $logDir)) {
		New-Item -ItemType Directory -Path $logDir | Out-Null
	}
	$outLog = Join-Path $logDir 'smoke_generated_runtime_live.out.log'
	$errLog = Join-Path $logDir 'smoke_generated_runtime_live.err.log'
	$started = Start-Process -FilePath $ServerExe -WorkingDirectory $ServerCwd -WindowStyle Hidden -PassThru -RedirectStandardOutput $outLog -RedirectStandardError $errLog
}

try {
	if (!(Wait-LiveServer $BaseUrl $TimeoutSec $StartupTimeoutSec)) {
		throw "server is not reachable: $BaseUrl"
	}
	if (![string]::IsNullOrWhiteSpace($GenerateXid)) {
		$PluginXid = Invoke-ContentGenerate $BaseUrl $GenerateXid $TimeoutSec
	}
	if ($EnableGeneratedPlugin -and ![string]::IsNullOrWhiteSpace($PluginXid)) {
		try {
			Invoke-PluginAction $BaseUrl $PluginXid 'enable' $TimeoutSec | Out-Null
		} catch {
			if ([string]::IsNullOrWhiteSpace($AdminUsername) -and [string]::IsNullOrWhiteSpace($AdminPasswordHash)) {
				throw
			}
			Invoke-HostReload $BaseUrl $TimeoutSec | Out-Null
			Start-Sleep -Seconds 2
			Update-SmokeCookieFromLogin $BaseUrl $TimeoutSec
			if (!(Wait-LiveServer $BaseUrl $TimeoutSec $StartupTimeoutSec)) {
				throw "server is not reachable after host reload: $BaseUrl"
			}
			Invoke-PluginAction $BaseUrl $PluginXid 'enable' $TimeoutSec | Out-Null
		}
		Invoke-PluginAction $BaseUrl $PluginXid 'reload' $TimeoutSec | Out-Null
	}
	if ([string]::IsNullOrWhiteSpace($RuntimeDir) -and ![string]::IsNullOrWhiteSpace($PluginXid)) {
		$RuntimeDir = Join-Path $Root ("hosts/xadmin/plugin/$PluginXid/runtime")
	}
	if ([string]::IsNullOrWhiteSpace($RuntimeDir)) {
		throw 'RuntimeDir is required unless GenerateXid or PluginXid can resolve hosts/xadmin/plugin/{pluginXid}/runtime'
	}
	$args = @(
		'-BaseUrl', $BaseUrl,
		'-RuntimeDir', $RuntimeDir,
		'-Root', $Root,
		'-AdminBase', $AdminBase,
		'-CookieHeader', $CookieHeader,
		'-TimeoutSec', $TimeoutSec
	)
	if (![string]::IsNullOrWhiteSpace($PluginXid)) { $args += @('-PluginXid', $PluginXid) }
	if ($SkipContentCheck) { $args += '-SkipContentCheck' }
	$script = Join-Path $PSScriptRoot 'smoke_generated_runtime.ps1'
	$result = & powershell -ExecutionPolicy Bypass -File $script @args
	$exitCode = $LASTEXITCODE
	$result
	exit $exitCode
} finally {
	if ($StopStartedServer -and $null -ne $started -and !$started.HasExited) {
		Stop-Process -Id $started.Id -Force
	}
}
