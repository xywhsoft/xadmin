param(
	[string]$BaseUrl = 'http://127.0.0.1',
	[string]$SpecPath = (Join-Path $PSScriptRoot 'fixtures/content_smoke_model.json'),
	[string]$Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path,
	[string]$AdminBase = '/admin',
	[string]$CookieHeader = '',
	[string]$ServerExe = '',
	[string]$ServerCwd = '',
	[int]$TimeoutSec = 10,
	[int]$StartupTimeoutSec = 20,
	[switch]$StartServer,
	[switch]$StopStartedServer,
	[switch]$SkipContentCheck
)

$ErrorActionPreference = 'Stop'

function New-SmokeHeaders() {
	if ([string]::IsNullOrWhiteSpace($CookieHeader)) {
		return @{}
	}
	return @{ Cookie = $CookieHeader }
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
		$response = Invoke-WebRequest -Uri $url -Method Post -TimeoutSec $timeoutSec -UseBasicParsing -ContentType 'application/json' -Body $specText -Headers (New-SmokeHeaders)
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
	$adminBase = '/' + $AdminBase.Trim('/')
	try {
		Invoke-WebRequest -Uri ($baseUrl.TrimEnd('/') + $adminBase + '/content/types') -Method Get -TimeoutSec $timeoutSec -UseBasicParsing -Headers (New-SmokeHeaders) | Out-Null
		return $true
	} catch {
		if ($_.Exception.Response) {
			$status = [int]$_.Exception.Response.StatusCode
			return $status -ne 404
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
