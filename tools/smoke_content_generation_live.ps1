param(
	[string]$BaseUrl = 'http://127.0.0.1',
	[string]$SpecPath = (Join-Path $PSScriptRoot 'fixtures/content_smoke_model.json'),
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

function Invoke-ContentSave($baseUrl, $specPath, $timeoutSec) {
	if (!(Test-Path $specPath)) {
		throw "SpecPath not found: $specPath"
	}
	$specText = Get-Content -Raw -Encoding UTF8 $specPath
	$spec = $specText | ConvertFrom-Json
	$xid = [string]$spec.xid
	if ([string]::IsNullOrWhiteSpace($xid)) {
		throw 'fixture spec missing xid'
	}
	$adminBase = '/' + $AdminBase.Trim('/')
	$url = $baseUrl.TrimEnd('/') + $adminBase + '/content/save'
	try {
		$response = Invoke-WebRequest -Uri $url -Method Post -TimeoutSec $timeoutSec -UseBasicParsing -ContentType 'application/json' -Body $specText -WebSession $SmokeWebSession
		$json = [string]$response.Content | ConvertFrom-Json
		if ($true -ne [bool]$json.result) {
			throw "content save failed: $($json.message)"
		}
		return $xid
	} catch {
		throw "content save request failed: $url $($_.Exception.Message)"
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

$liveScript = Join-Path $PSScriptRoot 'smoke_generated_runtime_live.ps1'
$xid = ''
$started = $null
$SmokeWebSession = New-SmokeWebSession $BaseUrl

if ([string]::IsNullOrWhiteSpace($ServerCwd)) {
	$ServerCwd = $Root
}
if ([string]::IsNullOrWhiteSpace($ServerExe)) {
	$ServerExe = Join-Path $Root 'xs.exe'
}

if (!(Test-LiveServer $BaseUrl $TimeoutSec) -and $StartServer) {
	if (!(Test-Path $ServerExe)) {
		throw "ServerExe not found: $ServerExe"
	}
	$logDir = Join-Path $Root 'data/logs'
	if (!(Test-Path $logDir)) {
		New-Item -ItemType Directory -Path $logDir | Out-Null
	}
	$outLog = Join-Path $logDir 'smoke_content_generation_live.out.log'
	$errLog = Join-Path $logDir 'smoke_content_generation_live.err.log'
	$started = Start-Process -FilePath $ServerExe -WorkingDirectory $ServerCwd -WindowStyle Hidden -PassThru -RedirectStandardOutput $outLog -RedirectStandardError $errLog
}

try {
	if (!(Wait-LiveServer $BaseUrl $TimeoutSec $StartupTimeoutSec)) {
		throw "server is not reachable: $BaseUrl"
	}

	if ([string]::IsNullOrWhiteSpace($CookieHeader) -and (![string]::IsNullOrWhiteSpace($AdminUsername) -or ![string]::IsNullOrWhiteSpace($AdminPasswordHash))) {
		Update-SmokeCookieFromLogin $BaseUrl $TimeoutSec
	}

	if (($null -eq $started) -and (![string]::IsNullOrWhiteSpace($AdminUsername) -or ![string]::IsNullOrWhiteSpace($AdminPasswordHash))) {
		Invoke-HostReload $BaseUrl $TimeoutSec | Out-Null
		Start-Sleep -Seconds 2
		Update-SmokeCookieFromLogin $BaseUrl $TimeoutSec
		if (!(Wait-LiveServer $BaseUrl $TimeoutSec $StartupTimeoutSec)) {
			throw "server is not reachable after host reload: $BaseUrl"
		}
	}

	$xid = Invoke-ContentSave $BaseUrl $SpecPath $TimeoutSec

	$smokeArgs = @(
		'-BaseUrl', $BaseUrl,
		'-Root', $Root,
		'-AdminBase', $AdminBase,
		'-CookieHeader', $CookieHeader,
		'-TimeoutSec', $TimeoutSec,
		'-StartupTimeoutSec', $StartupTimeoutSec,
		'-GenerateXid', $xid,
		'-EnableGeneratedPlugin'
	)
	if (![string]::IsNullOrWhiteSpace($AdminLoginPath)) { $smokeArgs += @('-AdminLoginPath', $AdminLoginPath) }
	if (![string]::IsNullOrWhiteSpace($AdminUsername)) { $smokeArgs += @('-AdminUsername', $AdminUsername) }
	if (![string]::IsNullOrWhiteSpace($AdminPassword)) { $smokeArgs += @('-AdminPassword', $AdminPassword) }
	if (![string]::IsNullOrWhiteSpace($AdminPasswordHash)) { $smokeArgs += @('-AdminPasswordHash', $AdminPasswordHash) }
	if ($RememberLogin) { $smokeArgs += '-RememberLogin' }
	if (![string]::IsNullOrWhiteSpace($ServerExe)) { $smokeArgs += @('-ServerExe', $ServerExe) }
	if (![string]::IsNullOrWhiteSpace($ServerCwd)) { $smokeArgs += @('-ServerCwd', $ServerCwd) }
	if ($SkipContentCheck) { $smokeArgs += '-SkipContentCheck' }

	& powershell -ExecutionPolicy Bypass -File $liveScript @smokeArgs
	exit $LASTEXITCODE
} finally {
	if ($StopStartedServer -and $null -ne $started -and !$started.HasExited) {
		Stop-Process -Id $started.Id -Force
	}
}
