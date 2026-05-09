param(
	[string]$Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
)

$ErrorActionPreference = 'Stop'

$packRoot = Join-Path $Root 'hosts/xadmin/capability-pack'
$advisorFile = Join-Path $Root 'hosts/xadmin/script/content/content_advisor.h'
$managedTemplate = Join-Path $Root 'hosts/xadmin/data/content/templates/managed_main.c.tpl'
$errors = New-Object System.Collections.Generic.List[string]
$warnings = New-Object System.Collections.Generic.List[string]

function Read-JsonFile($path) {
	(Get-Content -Raw -Encoding UTF8 $path) | ConvertFrom-Json
}

function Add-Error($message) {
	$script:errors.Add($message) | Out-Null
}

function Add-Warning($message) {
	$script:warnings.Add($message) | Out-Null
}

function Test-PackPathList($packId, $dir, $owner, $name) {
	$values = @($owner.$name)
	foreach ($value in $values) {
		$text = [string]$value
		if ([string]::IsNullOrWhiteSpace($text)) { continue }
		if ([System.IO.Path]::IsPathRooted($text)) {
			Add-Error "$packId $name must be relative: $text"
			continue
		}
		if ($text -match '(^|[\\/])\.\.([\\/]|$)') {
			Add-Error "$packId $name must stay inside pack directory: $text"
			continue
		}
		$fullPath = Join-Path $dir.FullName $text
		if (!(Test-Path $fullPath)) {
			Add-Error "$packId $name path not found: $text"
		}
	}
}

function Test-PackReferenceFile($packId, $dir, $owner, $name, $defaultName) {
	$text = [string]$owner.$name
	if ([string]::IsNullOrWhiteSpace($text)) {
		$text = $defaultName
	}
	if ([string]::IsNullOrWhiteSpace($text)) {
		return $null
	}
	if ([System.IO.Path]::IsPathRooted($text)) {
		Add-Error "$packId $name must be relative: $text"
		return $null
	}
	if ($text -match '(^|[\\/])\.\.([\\/]|$)') {
		Add-Error "$packId $name must stay inside pack directory: $text"
		return $null
	}
	$fullPath = Join-Path $dir.FullName $text
	if (!(Test-Path $fullPath)) {
		return $null
	}
	return $fullPath
}

if (!(Test-Path $packRoot)) {
	throw "capability-pack directory not found: $packRoot"
}

$advisorText = if (Test-Path $advisorFile) { Get-Content -Raw -Encoding UTF8 $advisorFile } else { '' }
$templateText = if (Test-Path $managedTemplate) { Get-Content -Raw -Encoding UTF8 $managedTemplate } else { '' }
$packDirs = Get-ChildItem -Path $packRoot -Directory | Sort-Object Name
$builtinTables = @(
	'content_category'
)

foreach ($dir in $packDirs) {
	$packId = $dir.Name
	$packJsonPath = Join-Path $dir.FullName 'pack.json'
	$contractsPath = Join-Path $dir.FullName 'contracts.json'
	$effectsPath = Join-Path $dir.FullName 'effects.json'
	$schemaPath = Join-Path $dir.FullName 'schema.sql'

	foreach ($required in @($packJsonPath, $contractsPath, $effectsPath)) {
		if (!(Test-Path $required)) {
			Add-Error "$packId missing $(Split-Path $required -Leaf)"
		}
	}
	if (!(Test-Path $packJsonPath) -or !(Test-Path $contractsPath) -or !(Test-Path $effectsPath)) {
		continue
	}

	try {
		$pack = Read-JsonFile $packJsonPath
		$contracts = Read-JsonFile $contractsPath
		$effects = Read-JsonFile $effectsPath
	} catch {
		Add-Error "$packId json parse failed: $($_.Exception.Message)"
		continue
	}

	if ($pack.packId -ne $packId) {
		Add-Error "$packId pack.json packId mismatch: $($pack.packId)"
	}
	if ([string]::IsNullOrWhiteSpace($pack.title)) {
		Add-Error "$packId missing display title"
	}
	if ($pack.title -eq $packId -or $pack.title -eq $pack.name) {
		Add-Warning "$packId title looks non-localized: $($pack.title)"
	}
	foreach ($name in @('sourceFiles', 'includeFiles', 'templateFiles', 'assetFiles')) {
		Test-PackPathList $packId $dir $pack $name
	}
	$patchesPath = Test-PackReferenceFile $packId $dir $pack 'patches' 'patches.json'
	if ($patchesPath) {
		try {
			$patches = Read-JsonFile $patchesPath
			$operations = @($patches.operations)
			$isAdvanced = ($patches.advanced -eq $true)
			if ($operations.Count -gt 0 -and !$isAdvanced) {
				Add-Error "$packId patches.json has operations but advanced is not true; patches are not a default generation path"
			}
		} catch {
			Add-Error "$packId patches.json parse failed: $($_.Exception.Message)"
		}
	}

	$permissions = @($contracts.permissions)
	$adminPages = @($contracts.adminPages)
	$tables = @($contracts.tables)
	if ($permissions.Count -eq 0 -or [string]::IsNullOrWhiteSpace([string]$permissions[0])) {
		Add-Error "$packId contracts.permissions is empty"
	}
	if ($adminPages.Count -eq 0 -or [string]::IsNullOrWhiteSpace([string]$adminPages[0])) {
		Add-Error "$packId contracts.adminPages is empty"
	}
	foreach ($table in $tables) {
		if ([string]::IsNullOrWhiteSpace([string]$table)) { continue }
		if ($builtinTables -contains $table) { continue }
		if (!(Test-Path $schemaPath)) {
			Add-Error "$packId declares table '$table' but schema.sql is missing"
			break
		}
		$schemaText = Get-Content -Raw -Encoding UTF8 $schemaPath
		if ($schemaText -notmatch [regex]::Escape([string]$table)) {
			Add-Error "$packId schema.sql does not mention declared table '$table'"
		}
	}

	$effectsTables = @($effects.tables)
	foreach ($table in $tables) {
		if ([string]::IsNullOrWhiteSpace([string]$table)) { continue }
		if (($effectsTables -notcontains $table)) {
			Add-Warning "$packId effects.tables does not include '$table'"
		}
	}

	foreach ($name in @('advisorMessage', 'acceptanceApiPath', 'acceptanceViewPath')) {
		if ([string]::IsNullOrWhiteSpace([string]$pack.$name)) {
			Add-Error "$packId manifest advisor metadata missing $name"
		}
	}
	if ($pack.acceptanceApiPath -and ([string]$pack.acceptanceApiPath -notmatch '\{pluginXid\}')) {
		Add-Error "$packId acceptanceApiPath must include {pluginXid}"
	}
	if ($pack.acceptanceViewPath -and ([string]$pack.acceptanceViewPath -notmatch '\{pluginXid\}')) {
		Add-Error "$packId acceptanceViewPath must include {pluginXid}"
	}
	if ($advisorText -notmatch [regex]::Escape("{pluginXid}")) {
		Add-Error "advisor acceptance api path placeholder missing"
	}
	if ($templateText -notmatch [regex]::Escape($packId)) {
		Add-Warning "$packId is not referenced by managed_main.c.tpl; verify it is generator-only or legacy-driven"
	}
}

$result = [pscustomobject]@{
	PackCount = $packDirs.Count
	ErrorCount = $errors.Count
	WarningCount = $warnings.Count
	Errors = @($errors)
	Warnings = @($warnings)
}

$result | ConvertTo-Json -Depth 6
if ($errors.Count -gt 0) {
	exit 1
}
