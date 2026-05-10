param(
	[string]$BaseUrl = 'http://127.0.0.1:8080',
	[string]$PluginXid,
	[string[]]$PackId = @(),
	[string]$RuntimeDir,
	[string]$ManifestPath,
	[string]$ManagedPath,
	[string]$ContractsPath,
	[string]$Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path,
	[string]$CookieHeader = '',
	[int]$TimeoutSec = 10,
	[switch]$ValidateManifestOnly,
	[switch]$SkipContentCheck
)

$ErrorActionPreference = 'Stop'

function Read-PackManifest($path) {
	(Get-Content -Raw -Encoding UTF8 $path) | ConvertFrom-Json
}

function New-SmokeHeaders() {
	if ([string]::IsNullOrWhiteSpace($CookieHeader)) {
		return @{}
	}
	return @{ Cookie = $CookieHeader }
}

function Resolve-RuntimeSiblingPath($manifestPath, $fileName) {
	if ([string]::IsNullOrWhiteSpace($manifestPath)) { return '' }
	Join-Path (Split-Path -Parent $manifestPath) $fileName
}

function Read-PackIdsFromRuntimeManifest($path) {
	if (!(Test-Path $path)) {
		throw "runtime capability manifest not found: $path"
	}
	$manifest = (Get-Content -Raw -Encoding UTF8 $path) | ConvertFrom-Json
	if ([string]$manifest.loadPolicy -ne 'enabled-packs-only') {
		throw "runtime capability manifest loadPolicy must be enabled-packs-only"
	}
	$ids = New-Object System.Collections.Generic.List[string]
	$seen = @{}
	foreach ($pack in @($manifest.abilityPacks)) {
		$id = [string]$pack.packId
		if (![string]::IsNullOrWhiteSpace($id)) {
			if ($seen.ContainsKey($id)) {
				throw "runtime capability manifest has duplicate packId: $id"
			}
			$seen[$id] = $true
			$ids.Add($id) | Out-Null
		} else {
			throw 'runtime capability manifest abilityPacks item missing packId'
		}
	}
	return @($ids)
}

function Read-PackIdsFromRuntimeManaged($path) {
	if (!(Test-Path $path)) {
		throw "runtime managed json not found: $path"
	}
	$managed = (Get-Content -Raw -Encoding UTF8 $path) | ConvertFrom-Json
	if ($true -ne [bool]$managed.managed) {
		throw 'runtime managed json must declare managed=true'
	}
	$ids = New-Object System.Collections.Generic.List[string]
	$seen = @{}
	foreach ($slot in @($managed.capabilitySlots)) {
		$id = [string]$(if (![string]::IsNullOrWhiteSpace([string]$slot.key)) { $slot.key } else { $slot.packId })
		if ([string]::IsNullOrWhiteSpace($id)) {
			throw 'runtime managed capabilitySlots item missing key/packId'
		}
		if ($seen.ContainsKey($id)) {
			throw "runtime managed capabilitySlots has duplicate packId: $id"
		}
		$seen[$id] = $true
		$ids.Add($id) | Out-Null
	}
	return @($ids)
}

function Read-PackIdsFromRuntimeContracts($path) {
	if (!(Test-Path $path)) {
		throw "runtime contracts json not found: $path"
	}
	$contracts = (Get-Content -Raw -Encoding UTF8 $path) | ConvertFrom-Json
	$packIds = New-Object System.Collections.Generic.List[string]
	$capIds = New-Object System.Collections.Generic.List[string]
	foreach ($pack in @($contracts.abilityPacks)) {
		$id = [string]$pack.packId
		if ([string]::IsNullOrWhiteSpace($id)) {
			throw 'runtime contracts abilityPacks item missing packId'
		}
		$packIds.Add($id) | Out-Null
	}
	foreach ($cap in @($contracts.capabilities)) {
		$id = [string]$(if (![string]::IsNullOrWhiteSpace([string]$cap.key)) { $cap.key } else { $cap.packId })
		if ([string]::IsNullOrWhiteSpace($id)) {
			throw 'runtime contracts capabilities item missing key/packId'
		}
		$capIds.Add($id) | Out-Null
	}
	Assert-SamePackIds @($packIds) @($capIds) 'runtime contracts abilityPacks/capabilities'
	return @($packIds)
}

function Assert-SamePackIds($expected, $actual, $label) {
	$left = @($expected | Sort-Object)
	$right = @($actual | Sort-Object)
	if ($left.Count -ne $right.Count) {
		throw "$label pack count mismatch: expected=$($left.Count) actual=$($right.Count)"
	}
	for ($i = 0; $i -lt $left.Count; $i++) {
		if ($left[$i] -ne $right[$i]) {
			throw "$label pack mismatch: expected=$($left -join ',') actual=$($right -join ',')"
		}
	}
}

function Test-SmokeUrl($url, $timeoutSec, $kind) {
	$status = 0
	$errorText = ''
	$contentText = ''
	$contentOk = $true
	try {
		$response = Invoke-WebRequest -Uri $url -Method Get -TimeoutSec $timeoutSec -UseBasicParsing -Headers (New-SmokeHeaders)
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
			if ($contentOk) {
				try {
					$contentText | ConvertFrom-Json | Out-Null
				} catch {
					$contentOk = $false
					$errorText = "response is not valid JSON: $($_.Exception.Message)"
				}
			} else {
				$errorText = 'response does not look like JSON'
			}
		} elseif ($kind -eq 'view') {
			$contentOk = ($contentText -match '<html|<!DOCTYPE html|layui|managed-')
			if (!$contentOk) {
				$errorText = 'response does not look like an xAdmin HTML page'
			}
		}
		$ok = $ok -and $contentOk
	}
	[pscustomobject]@{
		url = $url
		status = $status
		ok = $ok
		error = $errorText
		contentOk = $contentOk
	}
}

function Resolve-AcceptancePath($packId, $path, $kind) {
	if ([string]::IsNullOrWhiteSpace($path)) {
		if ($kind -eq 'view') {
			return ''
		}
		throw "$packId acceptance $kind path is empty"
	}
	$rawPath = [string]$path
	if (!$rawPath.StartsWith('/')) {
		throw "$packId acceptance $kind path must be site-relative: $rawPath"
	}
	if ($rawPath -match '^(?i:https?:)?//') {
		throw "$packId acceptance $kind path must not be an external URL: $rawPath"
	}
	if ($rawPath -notmatch '\{pluginXid\}') {
		throw "$packId acceptance $kind path must include {pluginXid}: $rawPath"
	}
	return $rawPath.Replace('{pluginXid}', [uri]::EscapeDataString($PluginXid))
}

function Normalize-PackIdList($ids, $source) {
	$list = New-Object System.Collections.Generic.List[string]
	$seen = @{}
	foreach ($raw in @($ids)) {
		$id = [string]$raw
		if ([string]::IsNullOrWhiteSpace($id)) {
			throw "$source contains empty packId"
		}
		if ($id -notmatch '^[a-z][a-z0-9-]*(\.[a-z][a-z0-9-]*)+$') {
			throw "$source contains invalid packId: $id"
		}
		if ($seen.ContainsKey($id)) {
			throw "$source contains duplicate packId: $id"
		}
		$seen[$id] = $true
		$list.Add($id) | Out-Null
	}
	return @($list)
}

if ([string]::IsNullOrWhiteSpace($PluginXid)) {
	throw 'PluginXid is required'
}

if (![string]::IsNullOrWhiteSpace($RuntimeDir)) {
	if (!(Test-Path $RuntimeDir)) {
		throw "RuntimeDir not found: $RuntimeDir"
	}
	if ([string]::IsNullOrWhiteSpace($ManifestPath)) {
		$ManifestPath = Join-Path $RuntimeDir 'capability.manifest.json'
	}
	if ([string]::IsNullOrWhiteSpace($ManagedPath)) {
		$ManagedPath = Join-Path $RuntimeDir 'managed.json'
	}
	if ([string]::IsNullOrWhiteSpace($ContractsPath)) {
		$ContractsPath = Join-Path $RuntimeDir 'contracts.json'
	}
}

$packRoot = Join-Path $Root 'hosts/xadmin/capability-pack'
if (!(Test-Path $packRoot)) {
	throw "capability-pack directory not found: $packRoot"
}

$acceptance = @{}
Get-ChildItem -Path $packRoot -Directory | Sort-Object Name | ForEach-Object {
	$packJson = Join-Path $_.FullName 'pack.json'
	if (!(Test-Path $packJson)) { return }
	$pack = Read-PackManifest $packJson
	if (![string]::IsNullOrWhiteSpace([string]$pack.packId) -and ![string]::IsNullOrWhiteSpace([string]$pack.acceptanceApiPath)) {
		$acceptance[[string]$pack.packId] = [pscustomobject]@{
			apiPath = [string]$pack.acceptanceApiPath
			viewPath = [string]$pack.acceptanceViewPath
		}
	}
}

if ($PackId.Count -eq 0) {
	if (![string]::IsNullOrWhiteSpace($ManifestPath)) {
		$PackId = @(Read-PackIdsFromRuntimeManifest $ManifestPath)
	} else {
		$PackId = $acceptance.Keys | Sort-Object
	}
}
$PackId = @(Normalize-PackIdList $PackId 'PackId')

if (![string]::IsNullOrWhiteSpace($ManifestPath)) {
	if ([string]::IsNullOrWhiteSpace($ManagedPath)) {
		$ManagedPath = Resolve-RuntimeSiblingPath $ManifestPath 'managed.json'
	}
	if ([string]::IsNullOrWhiteSpace($ContractsPath)) {
		$ContractsPath = Resolve-RuntimeSiblingPath $ManifestPath 'contracts.json'
	}
	$managedPackIds = @(Normalize-PackIdList (Read-PackIdsFromRuntimeManaged $ManagedPath) 'runtime/managed.json capabilitySlots')
	$contractPackIds = @(Normalize-PackIdList (Read-PackIdsFromRuntimeContracts $ContractsPath) 'runtime/contracts.json abilityPacks')
	Assert-SamePackIds $PackId $managedPackIds 'runtime manifest/managed enabled pack list'
	Assert-SamePackIds $PackId $contractPackIds 'runtime manifest/contracts enabled pack list'
} elseif ($ValidateManifestOnly) {
	throw 'ValidateManifestOnly requires ManifestPath'
}

if ($ValidateManifestOnly) {
	[pscustomobject]@{
		PluginXid = $PluginXid
		RuntimeDir = $RuntimeDir
		ManifestPath = $ManifestPath
		ManagedPath = $ManagedPath
		ContractsPath = $ContractsPath
		ValidateManifestOnly = $true
		Checked = 0
		ErrorCount = 0
		PackIds = @($PackId)
		Errors = @()
	} | ConvertTo-Json -Depth 6
	exit 0
}

$base = $BaseUrl.TrimEnd('/')
$results = New-Object System.Collections.Generic.List[object]
$errors = New-Object System.Collections.Generic.List[string]
$apiSuccessCount = 0
$apiAuthCount = 0
$apiRedirectCount = 0
$viewSuccessCount = 0
$viewAuthCount = 0
$viewRedirectCount = 0
$viewCheckedCount = 0

foreach ($pack in $PackId) {
	if (!$acceptance.ContainsKey($pack)) {
		$errors.Add("$pack has no acceptance path") | Out-Null
		continue
	}
	$entry = $acceptance[$pack]
	$path = Resolve-AcceptancePath $pack $entry.apiPath 'api'
	$viewPath = Resolve-AcceptancePath $pack $entry.viewPath 'view'
	$url = $base + $path
	$viewUrl = if (![string]::IsNullOrWhiteSpace($viewPath)) { $base + $viewPath } else { '' }
	$apiResult = Test-SmokeUrl $url $TimeoutSec 'api'
	$viewResult = if (![string]::IsNullOrWhiteSpace($viewUrl)) { Test-SmokeUrl $viewUrl $TimeoutSec 'view' } else { $null }
	$ok = $apiResult.ok -and (($null -eq $viewResult) -or $viewResult.ok)
	if ($apiResult.status -ge 200 -and $apiResult.status -lt 300) {
		$apiSuccessCount++
	} elseif ($apiResult.status -ge 300 -and $apiResult.status -lt 400) {
		$apiRedirectCount++
	} elseif ($apiResult.status -eq 401 -or $apiResult.status -eq 403) {
		$apiAuthCount++
	}
	if ($null -ne $viewResult) {
		$viewCheckedCount++
		if ($viewResult.status -ge 200 -and $viewResult.status -lt 300) {
			$viewSuccessCount++
		} elseif ($viewResult.status -ge 300 -and $viewResult.status -lt 400) {
			$viewRedirectCount++
		} elseif ($viewResult.status -eq 401 -or $viewResult.status -eq 403) {
			$viewAuthCount++
		}
	}
	if (!$apiResult.ok) {
		$errors.Add("$pack api failed: status=$($apiResult.status) url=$url $($apiResult.error)") | Out-Null
	}
	if (($null -ne $viewResult) -and !$viewResult.ok) {
		$errors.Add("$pack view failed: status=$($viewResult.status) url=$viewUrl $($viewResult.error)") | Out-Null
	}
	$results.Add([pscustomobject]@{
		packId = $pack
		url = $url
		viewUrl = $viewUrl
		status = $apiResult.status
		viewStatus = if ($null -ne $viewResult) { $viewResult.status } else { 0 }
		ok = $ok
		error = $apiResult.error
		viewError = if ($null -ne $viewResult) { $viewResult.error } else { '' }
		contentOk = $apiResult.contentOk
		viewContentOk = if ($null -ne $viewResult) { $viewResult.contentOk } else { $true }
	}) | Out-Null
}

[pscustomobject]@{
	PluginXid = $PluginXid
	BaseUrl = $base
	RuntimeDir = $RuntimeDir
	ManifestPath = $ManifestPath
	ContentCheck = !$SkipContentCheck
	Checked = $results.Count
	Summary = [pscustomobject]@{
		api2xx = $apiSuccessCount
		api3xx = $apiRedirectCount
		apiAuth = $apiAuthCount
		viewChecked = $viewCheckedCount
		view2xx = $viewSuccessCount
		view3xx = $viewRedirectCount
		viewAuth = $viewAuthCount
	}
	ErrorCount = $errors.Count
	Results = @($results)
	Errors = @($errors)
} | ConvertTo-Json -Depth 6

if ($errors.Count -gt 0) {
	exit 1
}
