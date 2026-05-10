param(
	[string]$Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path,
	[bool]$FailOnWarning = $true
)

$ErrorActionPreference = 'Stop'

$packRoot = Join-Path $Root 'hosts/xadmin/capability-pack'
$advisorFile = Join-Path $Root 'hosts/xadmin/script/content/content_advisor.h'
$generatorFile = Join-Path $Root 'hosts/xadmin/script/content/content_generator.h'
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

function Test-TextNoMojibake($packId, $path, $kind) {
	$text = Get-Content -Raw -Encoding UTF8 $path
	$badChars = @(
		[string][char]0x6769,
		[string][char]0x741B,
		[string][char]0x93C4,
		[string][char]0x95C4,
		[string][char]0x6D93,
		[string][char]0x6D94,
		[string][char]0x8FA9,
		[string][char]0x721C
	)
	foreach ($bad in $badChars) {
		if ($text.Contains($bad)) {
			Add-Error "$packId $kind contains possible mojibake: U+$(([int][char]$bad).ToString('X4'))"
		}
	}
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

function Test-XFormFile($packId, $path) {
	Test-TextNoMojibake $packId $path 'instance.xform.json'
	try {
		$form = Read-JsonFile $path
	} catch {
		Add-Error "$packId instance.xform.json parse failed: $($_.Exception.Message)"
		return @()
	}
	$fields = @()
	if ($form.fields) {
		$fields += @($form.fields)
	}
	foreach ($group in @($form.groups)) {
		if ($group -and $group.fields) {
			$groupTitle = [string]$group.title
			if ([string]::IsNullOrWhiteSpace($groupTitle)) {
				Add-Warning "$packId instance.xform.json group '$($group.key)' missing title"
			} elseif ($groupTitle -notmatch $script:hanTextPattern) {
				Add-Error "$packId instance.xform.json group '$($group.key)' title must contain Chinese text"
			} elseif ($groupTitle -match '^[A-Za-z]') {
				Add-Error "$packId instance.xform.json group '$($group.key)' title must not start with English text"
			}
			$fields += @($group.fields)
		}
	}
	if ($fields.Count -eq 0) {
		Add-Error "$packId instance.xform.json has no fields"
		return @()
	}
	$fieldNames = @{}
	foreach ($field in $fields) {
		if ([string]::IsNullOrWhiteSpace([string]$field.name)) {
			Add-Error "$packId instance.xform.json field missing name"
		} else {
			$fieldName = [string]$field.name
			if ($fieldNames.ContainsKey($fieldName)) {
				Add-Error "$packId instance.xform.json duplicate field name '$fieldName'"
			} else {
				$fieldNames[$fieldName] = $true
			}
		}
		if ([string]::IsNullOrWhiteSpace([string]$field.label)) {
			Add-Warning "$packId instance.xform.json field '$($field.name)' missing label"
		} else {
			$label = [string]$field.label
			if ($label -notmatch $script:hanTextPattern) {
				Add-Error "$packId instance.xform.json field '$($field.name)' label must contain Chinese text"
			}
			if ($label -match '^[A-Za-z]') {
				Add-Error "$packId instance.xform.json field '$($field.name)' label must not start with English text"
			}
		}
		foreach ($item in @($field.list)) {
			$itemLabel = [string]$item.label
			if ([string]::IsNullOrWhiteSpace($itemLabel)) { continue }
			if ($itemLabel -notmatch $script:hanTextPattern) {
				Add-Error "$packId instance.xform.json field '$($field.name)' option label must contain Chinese text"
			}
			if ($itemLabel -match '^[A-Za-z]') {
				Add-Error "$packId instance.xform.json field '$($field.name)' option label must not start with English text"
			}
		}
	}
	return @($fields | ForEach-Object { [string]$_.name } | Where-Object { ![string]::IsNullOrWhiteSpace($_) })
}

function Test-SchemaFile($packId, $path, $declaredTables) {
	if (!(Test-Path $path)) {
		return
	}
	Test-TextNoMojibake $packId $path 'schema.sql'
	$schemaText = Get-Content -Raw -Encoding UTF8 $path
	if ([string]::IsNullOrWhiteSpace($schemaText)) {
		Add-Error "$packId schema.sql is empty"
		return
	}
	if ($schemaText -match '\$\{') {
		Add-Error "$packId schema.sql contains unresolved template placeholder"
	}
	if (!$schemaText.TrimEnd().EndsWith(';')) {
		Add-Error "$packId schema.sql must end with a semicolon"
	}
	$openCount = ([regex]::Matches($schemaText, '\(')).Count
	$closeCount = ([regex]::Matches($schemaText, '\)')).Count
	if ($openCount -ne $closeCount) {
		Add-Error "$packId schema.sql has unbalanced parentheses"
	}

	$declared = @($declaredTables) | ForEach-Object { [string]$_ } | Where-Object { ![string]::IsNullOrWhiteSpace($_) }
	$createdTables = New-Object System.Collections.Generic.List[string]
	$statements = $schemaText -split ';'
	foreach ($statement in $statements) {
		$sql = $statement.Trim()
		if ([string]::IsNullOrWhiteSpace($sql)) { continue }
		$match = [regex]::Match($sql, '^(?is)CREATE\s+(?:(UNIQUE)\s+)?(TABLE|INDEX)\s+IF\s+NOT\s+EXISTS\s+([A-Za-z_][A-Za-z0-9_]*)\b')
		if (!$match.Success) {
			Add-Error "$packId schema.sql has unsupported statement: $($sql.Substring(0, [Math]::Min(80, $sql.Length)))"
			continue
		}
		$kind = $match.Groups[2].Value.ToUpperInvariant()
		$name = $match.Groups[3].Value
		if ($kind -eq 'TABLE') {
			$createdTables.Add($name) | Out-Null
		} elseif ($kind -eq 'INDEX') {
			if ($script:schemaIndexNames.ContainsKey($name)) {
				Add-Error "$packId schema.sql duplicate index name '$name'; already used by $($script:schemaIndexNames[$name])"
			} else {
				$script:schemaIndexNames[$name] = $packId
			}
			if ($sql -notmatch '(?is)\bON\s+[A-Za-z_][A-Za-z0-9_]*\s*\(') {
				Add-Error "$packId schema.sql index '$name' missing ON table(...) clause"
			}
		}
	}
	foreach ($table in $declared) {
		if ($createdTables -notcontains $table) {
			Add-Error "$packId contracts table '$table' has no CREATE TABLE in schema.sql"
		}
	}
	foreach ($table in $createdTables) {
		if ($declared -notcontains $table) {
			Add-Error "$packId schema.sql creates table '$table' but contracts.tables does not declare it"
		}
	}
}

function Convert-AcceptancePathToApiKey($path) {
	$text = [string]$path
	if ([string]::IsNullOrWhiteSpace($text)) {
		return $null
	}
	$routePath = ($text -split '\?')[0]
	$scope = ''
	$relative = ''
	if ($routePath.StartsWith('/admin/api/plugin/{pluginXid}/')) {
		$scope = 'admin'
		$relative = $routePath.Substring('/admin/api/plugin/{pluginXid}/'.Length)
	} elseif ($routePath.StartsWith('/api/plugin/{pluginXid}/')) {
		$scope = 'public'
		$relative = $routePath.Substring('/api/plugin/{pluginXid}/'.Length)
	} else {
		return $null
	}
	if ([string]::IsNullOrWhiteSpace($relative)) {
		return $null
	}
	[pscustomobject]@{
		Scope = $scope
		Key = ($relative.Trim('/') -replace '/', '.')
		Path = $routePath
	}
}

function Convert-AcceptancePathToRoutePath($path) {
	$text = [string]$path
	if ([string]::IsNullOrWhiteSpace($text)) {
		return $null
	}
	$routePath = ($text -split '\?')[0]
	$routePath.Replace('{pluginXid}', '{{PLUGIN_XID}}')
}

function Convert-PackIdToSafeKey($packId) {
	$text = [string]$packId
	if ([string]::IsNullOrWhiteSpace($text)) {
		return ''
	}
	[regex]::Replace($text, '[^A-Za-z0-9_]', '_')
}

if (!(Test-Path $packRoot)) {
	throw "capability-pack directory not found: $packRoot"
}

$advisorText = if (Test-Path $advisorFile) { Get-Content -Raw -Encoding UTF8 $advisorFile } else { '' }
$generatorText = if (Test-Path $generatorFile) { Get-Content -Raw -Encoding UTF8 $generatorFile } else { '' }
$templateText = if (Test-Path $managedTemplate) { Get-Content -Raw -Encoding UTF8 $managedTemplate } else { '' }
$packDirs = Get-ChildItem -Path $packRoot -Directory | Sort-Object Name
$staticAuthSafeKeys = @{}
foreach ($match in [regex]::Matches($templateText, '\bint\s+auth_([A-Za-z0-9_]+)\s*=\s*0\s*;')) {
	$staticAuthSafeKeys[$match.Groups[1].Value] = $true
}
$builtinPackIds = @{}
$builtinMatch = [regex]::Match($generatorText, '(?s)bool\s+Content_IsBuiltinAbilityPack\s*\([^)]*\)\s*\{(?<body>.*?)\n\}')
if ($builtinMatch.Success) {
	foreach ($match in [regex]::Matches($builtinMatch.Groups['body'].Value, 'strcmp\s*\(\s*sPackId\s*,\s*"([^"]+)"\s*\)\s*==\s*0\s*\)\s*return\s+TRUE')) {
		$builtinPackIds[$match.Groups[1].Value] = $true
	}
} else {
	Add-Error "content_generator.h missing Content_IsBuiltinAbilityPack"
}
$permissionPackIds = @{}
$permissionMatch = [regex]::Match($generatorText, '(?s)const\s+char\*\s+Content_DefaultAbilityPermission\s*\([^)]*\)\s*\{(?<body>.*?)\n\}')
if ($permissionMatch.Success) {
	foreach ($match in [regex]::Matches($permissionMatch.Groups['body'].Value, 'strcmp\s*\(\s*sPackId\s*,\s*"([^"]+)"\s*\)\s*==\s*0\s*\)\s*return\s*"([^"]+)"')) {
		$permissionPackIds[$match.Groups[1].Value] = $match.Groups[2].Value
	}
} else {
	Add-Error "content_generator.h missing Content_DefaultAbilityPermission"
}
$menuTitlePackIds = @{}
$menuTitleMatch = [regex]::Match($generatorText, '(?s)const\s+char\*\s+Content_DefaultAbilityMenuTitle\s*\([^)]*\)\s*\{(?<body>.*?)\n\}')
if ($menuTitleMatch.Success) {
	foreach ($match in [regex]::Matches($menuTitleMatch.Groups['body'].Value, 'strcmp\s*\(\s*sPackId\s*,\s*"([^"]+)"\s*\)\s*==\s*0')) {
		$menuTitlePackIds[$match.Groups[1].Value] = $true
	}
} else {
	Add-Error "content_generator.h missing Content_DefaultAbilityMenuTitle"
}
$mojibakePattern = ([char]0x9365).ToString() + '|' + ([char]0x95be).ToString() + '|' + ([char]0x70ac).ToString() + '|' + ([char]0xfffd).ToString()
$builtinTables = @()
$schemaIndexNames = @{}
$permissionNames = @{}
$adminPageNames = @{}
$publicApiNames = @{}
$adminApiNames = @{}
$contractTableNames = @{}
$packIdPattern = '^[a-z][a-z0-9-]*(\.[a-z][a-z0-9-]*)+$'
$permissionPattern = '^[a-z][a-z0-9_]*(\.[a-z][a-z0-9_]*)+$'
$adminPagePattern = '^[a-z][a-z0-9_-]*$'
$hanTextPattern = '[\u4e00-\u9fff]'

foreach ($dir in $packDirs) {
	$packId = $dir.Name
	$packJsonPath = Join-Path $dir.FullName 'pack.json'
	$contractsPath = Join-Path $dir.FullName 'contracts.json'
	$effectsPath = Join-Path $dir.FullName 'effects.json'
	$schemaPath = Join-Path $dir.FullName 'schema.sql'
	$xformPath = Join-Path $dir.FullName 'instance.xform.json'

	foreach ($required in @($packJsonPath, $contractsPath, $effectsPath)) {
		if (!(Test-Path $required)) {
			Add-Error "$packId missing $(Split-Path $required -Leaf)"
		}
	}
	if (!(Test-Path $packJsonPath) -or !(Test-Path $contractsPath) -or !(Test-Path $effectsPath)) {
		continue
	}
	Test-TextNoMojibake $packId $packJsonPath 'pack.json'
	Test-TextNoMojibake $packId $contractsPath 'contracts.json'
	Test-TextNoMojibake $packId $effectsPath 'effects.json'
	$advisorPath = Join-Path $dir.FullName 'advisor.json'
	if (Test-Path $advisorPath) {
		Test-TextNoMojibake $packId $advisorPath 'advisor.json'
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
	if ($packId -notmatch $packIdPattern) {
		Add-Error "$packId packId must be a lowercase English dot-separated id"
	}
	$safePackKey = Convert-PackIdToSafeKey $packId
	if (!$permissionPackIds.ContainsKey($packId)) {
		Add-Error "$packId content_generator.h missing Content_DefaultAbilityPermission mapping"
	}
	if (!$menuTitlePackIds.ContainsKey($packId)) {
		Add-Error "$packId content_generator.h missing Content_DefaultAbilityMenuTitle mapping"
	}
	$hasStaticAuthVariable = $staticAuthSafeKeys.ContainsKey($safePackKey)
	$isBuiltinAuthPack = $builtinPackIds.ContainsKey($packId)
	if ($hasStaticAuthVariable -and !$isBuiltinAuthPack) {
		Add-Error "$packId has a static auth_$safePackKey variable in managed_main.c.tpl but is missing from Content_IsBuiltinAbilityPack"
	}
	if (!$hasStaticAuthVariable -and $isBuiltinAuthPack) {
		Add-Error "$packId is marked builtin in Content_IsBuiltinAbilityPack but managed_main.c.tpl has no static auth_$safePackKey variable"
	}
	if ([string]::IsNullOrWhiteSpace($pack.title)) {
		Add-Error "$packId missing display title"
	}
	if (![string]::IsNullOrWhiteSpace([string]$pack.title) -and ([string]$pack.title -notmatch $hanTextPattern)) {
		Add-Error "$packId display title must be localized Chinese text"
	}
	if (![string]::IsNullOrWhiteSpace([string]$pack.title) -and ([string]$pack.title -match '^[A-Za-z]')) {
		Add-Error "$packId display title must not start with English text"
	}
	if ($pack.title -eq $packId -or $pack.title -eq $pack.name) {
		Add-Warning "$packId title looks non-localized: $($pack.title)"
	}
	foreach ($name in @('description', 'advisorMessage')) {
		$text = [string]$pack.$name
		if ([string]::IsNullOrWhiteSpace($text)) {
			Add-Error "$packId manifest $name is empty"
		} elseif ($text -notmatch $hanTextPattern) {
			Add-Error "$packId manifest $name must contain Chinese text"
		} elseif ($text -match '^[A-Za-z]') {
			Add-Error "$packId manifest $name must not start with English text"
		}
	}
	foreach ($name in @('title', 'description', 'advisorMessage')) {
		if ([string]$pack.$name -match $mojibakePattern) {
			Add-Error "$packId manifest $name contains mojibake"
		}
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
	$xformFieldNames = @()
	if (Test-Path $xformPath) {
		$xformFieldNames = @(Test-XFormFile $packId $xformPath)
	} else {
		Add-Error "$packId missing instance.xform.json"
	}

	$permissions = @($contracts.permissions)
	$adminPages = @($contracts.adminPages)
	$publicApis = @($contracts.publicApis) | ForEach-Object { [string]$_ } | Where-Object { ![string]::IsNullOrWhiteSpace($_) }
	$adminApis = @($contracts.adminApis) | ForEach-Object { [string]$_ } | Where-Object { ![string]::IsNullOrWhiteSpace($_) }
	$tables = @($contracts.tables)
	if ($permissionPackIds.ContainsKey($packId)) {
		$defaultPermission = [string]$permissionPackIds[$packId]
		if ([string]::IsNullOrWhiteSpace($defaultPermission)) {
			Add-Error "$packId default ability permission is empty"
		} elseif (($permissions | ForEach-Object { [string]$_ }) -notcontains $defaultPermission) {
			Add-Error "$packId default ability permission '$defaultPermission' is missing from contracts.permissions"
		}
	}
	foreach ($apiKey in (@($publicApis) + @($adminApis))) {
		if ($apiKey -match '/') {
			Add-Error "$packId contracts API key must use dot-separated form, not slash path: $apiKey"
		}
		if ($apiKey -notmatch '^[a-z][a-z0-9-]*(\.[a-z][a-z0-9-]*)+$') {
			Add-Error "$packId contracts API key has invalid format: $apiKey"
		}
	}
	foreach ($apiKey in $publicApis) {
		if ($publicApiNames.ContainsKey($apiKey)) {
			Add-Error "$packId contracts.publicApis duplicate '$apiKey'; already used by $($publicApiNames[$apiKey])"
		} else {
			$publicApiNames[$apiKey] = $packId
		}
	}
	foreach ($apiKey in $adminApis) {
		if ($adminApiNames.ContainsKey($apiKey)) {
			Add-Error "$packId contracts.adminApis duplicate '$apiKey'; already used by $($adminApiNames[$apiKey])"
		} else {
			$adminApiNames[$apiKey] = $packId
		}
	}
	Test-SchemaFile $packId $schemaPath $tables
	foreach ($permission in $permissions) {
		$name = [string]$permission
		if ([string]::IsNullOrWhiteSpace($name)) { continue }
		if ($name -notmatch $permissionPattern) {
			Add-Error "$packId contracts.permissions has invalid format: $name"
		}
		if ($permissionNames.ContainsKey($name)) {
			Add-Error "$packId contracts.permissions duplicate '$name'; already used by $($permissionNames[$name])"
		} else {
			$permissionNames[$name] = $packId
		}
	}
	foreach ($adminPage in $adminPages) {
		$name = [string]$adminPage
		if ([string]::IsNullOrWhiteSpace($name)) { continue }
		if ($name -notmatch $adminPagePattern) {
			Add-Error "$packId contracts.adminPages has invalid format: $name"
		}
		if ($adminPageNames.ContainsKey($name)) {
			Add-Error "$packId contracts.adminPages duplicate '$name'; already used by $($adminPageNames[$name])"
		} else {
			$adminPageNames[$name] = $packId
		}
	}
	foreach ($tableName in $tables) {
		$name = [string]$tableName
		if ([string]::IsNullOrWhiteSpace($name) -or ($builtinTables -contains $name)) { continue }
		if ($contractTableNames.ContainsKey($name)) {
			Add-Error "$packId contracts.tables duplicate '$name'; already used by $($contractTableNames[$name])"
		} else {
			$contractTableNames[$name] = $packId
		}
	}
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
	$runtimeConfigKeys = @($contracts.runtime.configKeys) | ForEach-Object { [string]$_ } | Where-Object { ![string]::IsNullOrWhiteSpace($_) }
	$runtimeConfigKeySet = @{}
	foreach ($key in $runtimeConfigKeys) {
		if ([string]::IsNullOrWhiteSpace([string]$key)) { continue }
		if ($runtimeConfigKeySet.ContainsKey($key)) {
			Add-Error "$packId runtime.configKeys duplicate '$key'"
		} else {
			$runtimeConfigKeySet[$key] = $true
		}
		if ($xformFieldNames -notcontains [string]$key) {
			Add-Error "$packId runtime.configKeys '$key' has no matching instance.xform.json field"
		}
	}
	foreach ($fieldName in $xformFieldNames) {
		if ([string]::IsNullOrWhiteSpace([string]$fieldName)) { continue }
		if ($runtimeConfigKeys -notcontains [string]$fieldName) {
			Add-Error "$packId instance.xform.json field '$fieldName' is missing from runtime.configKeys"
		}
	}
	foreach ($fieldName in $xformFieldNames) {
		if ($runtimeConfigKeys -notcontains [string]$fieldName) {
			Add-Error "$packId instance.xform.json field '$fieldName' is not declared in runtime.configKeys"
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
	if ($pack.acceptanceApiPath -and ([string]$pack.acceptanceApiPath -notmatch '^/')) {
		Add-Error "$packId acceptanceApiPath must be a site-relative path"
	}
	if ($pack.acceptanceViewPath -and ([string]$pack.acceptanceViewPath -notmatch '^/')) {
		Add-Error "$packId acceptanceViewPath must be a site-relative path"
	}
	if ($pack.acceptanceApiPath -and ([string]$pack.acceptanceApiPath -match '^(?i:https?:)?//')) {
		Add-Error "$packId acceptanceApiPath must not be an external URL"
	}
	if ($pack.acceptanceViewPath -and ([string]$pack.acceptanceViewPath -match '^(?i:https?:)?//')) {
		Add-Error "$packId acceptanceViewPath must not be an external URL"
	}
	if ($pack.acceptanceApiPath -and ([string]$pack.acceptanceApiPath -match '/(save|delete|cleanup|refresh|rebuild|restore|action|batch|export)(/|\?|$)')) {
		Add-Error "$packId acceptanceApiPath must be a low-cost read/list/stats/check route"
	}
	if ($pack.acceptanceApiPath -and ([string]$pack.acceptanceApiPath -match '/revision/diff(/|\?|$)')) {
		Add-Error "$packId acceptanceApiPath must not use revision diff route"
	}
	if ($pack.acceptanceApiPath -and ([string]$pack.acceptanceApiPath -match '/redirect/resolve(/|\?|$)')) {
		Add-Error "$packId acceptanceApiPath must not use redirect resolve route"
	}
	$acceptanceKey = Convert-AcceptancePathToApiKey $pack.acceptanceApiPath
	if ($acceptanceKey -eq $null) {
		Add-Error "$packId acceptanceApiPath must be under /api/plugin/{pluginXid}/ or /admin/api/plugin/{pluginXid}/"
	} elseif ($acceptanceKey.Path -ne '/admin/api/plugin/{pluginXid}/pack/list') {
		if ($acceptanceKey.Scope -eq 'admin' -and ($adminApis -notcontains $acceptanceKey.Key)) {
			Add-Error "$packId acceptanceApiPath key '$($acceptanceKey.Key)' is missing from contracts.adminApis"
		}
		if ($acceptanceKey.Scope -eq 'public' -and ($publicApis -notcontains $acceptanceKey.Key)) {
			Add-Error "$packId acceptanceApiPath key '$($acceptanceKey.Key)' is missing from contracts.publicApis"
		}
	}
	$acceptanceRoutePath = Convert-AcceptancePathToRoutePath $pack.acceptanceApiPath
	if ($acceptanceRoutePath -and ($templateText -notmatch [regex]::Escape(('route.path = "' + $acceptanceRoutePath + '"')))) {
		Add-Error "$packId acceptanceApiPath route is not registered in managed_main.c.tpl: $acceptanceRoutePath"
	}
	$acceptanceViewPath = [string]$pack.acceptanceViewPath
	$expectedPackViewPath = '/admin/view/plugin/{pluginXid}/pack/' + (Convert-PackIdToSafeKey $packId)
	if ($acceptanceViewPath -match '^/admin/view/plugin/\{pluginXid\}/ability(/|$)') {
		Add-Error "$packId acceptanceViewPath uses stale /ability route; use the generated /pack/<safeKey> route"
	}
	if ($acceptanceViewPath.StartsWith('/admin/view/plugin/{pluginXid}/pack/')) {
		if ($acceptanceViewPath -ne $expectedPackViewPath) {
			Add-Error "$packId acceptanceViewPath must match generated pack route: $expectedPackViewPath"
		}
		if ($generatorText -notmatch [regex]::Escape('/admin/view/plugin/%s/pack/%s')) {
			Add-Error "$packId generated pack view route template is missing"
		}
		$safeKey = Convert-PackIdToSafeKey $packId
		$safeToPackMarker = "${safeKey}:'$packId'"
		if ($templateText -notmatch [regex]::Escape('Managed_RequestAbilityPackView')) {
			Add-Error "$packId generated pack view handler is missing"
		}
		$abilityTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_ability.html.tpl')
		if ($abilityTemplate -notmatch [regex]::Escape($safeToPackMarker)) {
			Add-Error "$packId managed ability page safeToPack is missing: $safeToPackMarker"
		}
	} else {
		$acceptanceViewRoutePath = Convert-AcceptancePathToRoutePath $acceptanceViewPath
		if ($acceptanceViewRoutePath -and ($templateText -notmatch [regex]::Escape(('route.path = "' + $acceptanceViewRoutePath + '"')))) {
			Add-Error "$packId acceptanceViewPath route is not registered in managed_main.c.tpl: $acceptanceViewRoutePath"
		}
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
if ($errors.Count -gt 0 -or ($FailOnWarning -and $warnings.Count -gt 0)) {
	exit 1
}
