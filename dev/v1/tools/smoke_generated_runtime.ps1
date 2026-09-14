param(
	[string]$BaseUrl = 'http://127.0.0.1:8080',
	[string]$PluginXid,
	[Parameter(Mandatory=$true)][string]$RuntimeDir,
	[string]$Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path,
	[string]$AdminBase = '/admin',
	[string]$CookieHeader = '',
	[int]$TimeoutSec = 10,
	[switch]$ValidateManifestOnly,
	[switch]$SkipContentCheck
)

$ErrorActionPreference = 'Stop'

function Read-JsonFile($path) {
	if (!(Test-Path $path)) {
		throw "required runtime file not found: $path"
	}
	(Get-Content -Raw -Encoding UTF8 $path) | ConvertFrom-Json
}

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

function Test-GeneratedUrl($url, $timeoutSec, $kind) {
	$status = 0
	$errorText = ''
	$contentText = ''
	$contentOk = $true
	try {
		$response = Invoke-WebRequest -Uri $url -Method Get -TimeoutSec $timeoutSec -UseBasicParsing -WebSession $SmokeWebSession
		$status = [int]$response.StatusCode
		$contentText = [string]$response.Content
	} catch {
		$status = if ($_.Exception.Response) { [int]$_.Exception.Response.StatusCode } else { 0 }
		$errorText = $_.Exception.Message
		try {
			if ($_.Exception.Response) {
				$stream = $_.Exception.Response.GetResponseStream()
				if ($stream) {
					$reader = New-Object System.IO.StreamReader($stream)
					$contentText = $reader.ReadToEnd()
					$reader.Close()
				}
			}
		} catch {
			$contentText = ''
		}
	}
	$ok = (($status -ge 200 -and $status -lt 400) -or $status -eq 401 -or $status -eq 403)
	if (!$SkipContentCheck -and $status -ge 200 -and $status -lt 300) {
		if ($kind -eq 'api') {
			$trimmed = $contentText.TrimStart()
			$contentOk = $trimmed.StartsWith('{') -or $trimmed.StartsWith('[')
			if (!$contentOk) { $errorText = 'response does not look like JSON' }
		} elseif ($kind -eq 'view') {
			$contentOk = ($contentText -match '<html|<!DOCTYPE html|layui|managed-')
			if (!$contentOk) { $errorText = 'response does not look like an xAdmin HTML page' }
		}
		$ok = $ok -and $contentOk
	}
	[pscustomobject]@{
		url = $url
		kind = $kind
		status = $status
		ok = $ok
		contentOk = $contentOk
		error = $errorText
	}
}

if (!(Test-Path $RuntimeDir)) {
	throw "RuntimeDir not found: $RuntimeDir"
}

$runtimeDirFull = (Resolve-Path $RuntimeDir).Path
$managedPath = Join-Path $runtimeDirFull 'managed.json'
$manifestPath = Join-Path $runtimeDirFull 'capability.manifest.json'
$contractsPath = Join-Path $runtimeDirFull 'contracts.json'
$managed = Read-JsonFile $managedPath
if ($true -ne [bool]$managed.managed) {
	throw 'runtime managed.json must declare managed=true'
}
if ([string]::IsNullOrWhiteSpace($PluginXid)) {
	$PluginXid = [string]$managed.pluginXid
}
if ([string]::IsNullOrWhiteSpace($PluginXid)) {
	$PluginXid = Split-Path (Split-Path -Parent $runtimeDirFull) -Leaf
}
if ([string]::IsNullOrWhiteSpace($PluginXid)) {
	throw 'PluginXid is required or must be present in runtime/managed.json'
}

$capabilityArgs = @(
	'-BaseUrl', $BaseUrl,
	'-PluginXid', $PluginXid,
	'-RuntimeDir', $runtimeDirFull,
	'-ManifestPath', $manifestPath,
	'-ManagedPath', $managedPath,
	'-ContractsPath', $contractsPath,
	'-Root', $Root,
	'-CookieHeader', $CookieHeader,
	'-TimeoutSec', $TimeoutSec
)
if ($ValidateManifestOnly) { $capabilityArgs += '-ValidateManifestOnly' }
if ($SkipContentCheck) { $capabilityArgs += '-SkipContentCheck' }

$capabilityScript = Join-Path $PSScriptRoot 'smoke_capability_acceptance.ps1'
$capabilityJson = & powershell -ExecutionPolicy Bypass -File $capabilityScript @capabilityArgs
$capabilityExit = $LASTEXITCODE
$capabilityResult = $capabilityJson | ConvertFrom-Json
if ($ValidateManifestOnly) {
	[pscustomobject]@{
		PluginXid = $PluginXid
		BaseUrl = $BaseUrl.TrimEnd('/')
		RuntimeDir = $runtimeDirFull
		ValidateManifestOnly = $true
		CoreChecked = 0
		Capability = $capabilityResult
		ErrorCount = if ($capabilityExit -eq 0) { 0 } else { 1 }
		Errors = if ($capabilityExit -eq 0) { @() } else { @('capability manifest validation failed') }
	} | ConvertTo-Json -Depth 8
	exit $capabilityExit
}

$base = $BaseUrl.TrimEnd('/')
$adminBase = '/' + $AdminBase.Trim('/')
$encodedXid = [uri]::EscapeDataString($PluginXid)
$SmokeWebSession = New-SmokeWebSession $BaseUrl
$corePaths = @(
	[pscustomobject]@{ kind = 'view'; path = "$adminBase/view/plugin/$encodedXid" },
	[pscustomobject]@{ kind = 'api'; path = "$adminBase/api/plugin/$encodedXid/list?limit=1" },
	[pscustomobject]@{ kind = 'api'; path = "/api/plugin/$encodedXid/list?limit=1" }
)
$coreResults = New-Object System.Collections.Generic.List[object]
$errors = New-Object System.Collections.Generic.List[string]
foreach ($probe in $corePaths) {
	$result = Test-GeneratedUrl ($base + $probe.path) $TimeoutSec $probe.kind
	$coreResults.Add($result) | Out-Null
	if (!$result.ok) {
		$errors.Add("core $($probe.kind) failed: status=$($result.status) url=$($result.url) $($result.error)") | Out-Null
	}
}
if ($capabilityExit -ne 0 -or [int]$capabilityResult.ErrorCount -gt 0) {
	foreach ($err in @($capabilityResult.Errors)) {
		$errors.Add("capability smoke failed: $err") | Out-Null
	}
}

[pscustomobject]@{
	PluginXid = $PluginXid
	BaseUrl = $base
	RuntimeDir = $runtimeDirFull
	ManagedPath = $managedPath
	ManifestPath = $manifestPath
	ContractsPath = $contractsPath
	ContentCheck = [bool](!$SkipContentCheck)
	CoreChecked = $coreResults.Count
	CoreResults = @($coreResults.ToArray())
	Capability = $capabilityResult
	ErrorCount = $errors.Count
	Errors = @($errors.ToArray())
} | ConvertTo-Json -Depth 8

if ($errors.Count -gt 0) {
	exit 1
}
