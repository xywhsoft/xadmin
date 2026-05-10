param(
	[string]$Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
)

$ErrorActionPreference = 'Stop'

function Invoke-Step($name, [scriptblock]$body) {
	Write-Output "== $name =="
	& $body
	Write-Output ""
}

function Invoke-NodeScript($name, $script) {
	$tmp = Join-Path $env:TEMP $name
	$enc = New-Object System.Text.UTF8Encoding($false)
	[System.IO.File]::WriteAllText($tmp, $script, $enc)
	try {
		node $tmp
		if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
	} finally {
		Remove-Item -Force $tmp -ErrorAction SilentlyContinue
	}
}

Invoke-Step 'capability packs' {
	powershell -ExecutionPolicy Bypass -File (Join-Path $Root 'tools/check_capability_packs.ps1') -Root $Root
	if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
	$packCheckScript = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'tools/check_capability_packs.ps1')
	foreach ($needle in @(
		'Test-SchemaFile',
		'schema.sql has unsupported statement',
		'contracts table',
		'duplicate index name',
		'contracts.permissions duplicate',
		'contracts.adminPages duplicate',
		'contracts.publicApis duplicate',
		'contracts.adminApis duplicate',
		'contracts.tables duplicate',
		'contracts.permissions has invalid format',
		'contracts.adminPages has invalid format',
		'instance.xform.json field',
		'is missing from runtime.configKeys',
		'uses stale /ability route',
		'generated pack view route template is missing',
		'managed ability page safeToPack is missing',
		'Content_IsBuiltinAbilityPack',
		'Content_DefaultAbilityPermission mapping',
		'default ability permission',
		'Content_DefaultAbilityMenuTitle mapping',
		'has a static auth_',
		'is marked builtin in Content_IsBuiltinAbilityPack'
	)) {
		if ($packCheckScript -notmatch [regex]::Escape($needle)) {
			throw "check_capability_packs.ps1 missing schema guard marker: $needle"
		}
	}
}

Invoke-Step 'content editor javascript' {
	$script = @'
const fs = require("fs");
const vm = require("vm");
for (const file of fs.readdirSync("hosts/xadmin/wwwroot/content/js").filter(f => f.endsWith(".js")).map(f => "hosts/xadmin/wwwroot/content/js/" + f)) {
  new vm.Script(fs.readFileSync(file, "utf8"), { filename: file });
  console.log(file + ": JS OK");
}
'@
	Invoke-NodeScript 'xadmin_content_editor_js_check.js' $script
}

Invoke-Step 'content editor page wiring' {
	$editorPage = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/page/content/editor.html')
	foreach ($needle in @('page_max_scan_rows', 'page_page_size')) {
		if ($editorPage -notmatch [regex]::Escape($needle)) {
			throw "content editor page missing marker: $needle"
		}
	}
}

Invoke-Step 'managed template javascript' {
	$script = @'
const fs = require("fs");
const vm = require("vm");
for (const file of [
  "hosts/xadmin/data/content/templates/managed_editor.html.tpl",
  "hosts/xadmin/data/content/templates/managed_ability.html.tpl",
  "hosts/xadmin/data/content/templates/managed_category.html.tpl",
  "hosts/xadmin/data/content/templates/managed_dashboard.html.tpl",
  "hosts/xadmin/data/content/templates/managed_tasks.html.tpl",
  "hosts/xadmin/data/content/templates/managed_public.html.tpl"
]) {
  const html = fs.readFileSync(file, "utf8");
  let checked = 0;
  let skipped = 0;
  for (const m of html.matchAll(/<script([^>]*)>([\s\S]*?)<\/script>/gi)) {
    const attrs = (m[1] || "").toLowerCase();
    if (attrs.includes("text/html") || attrs.includes("text/template")) {
      skipped++;
      continue;
    }
    checked++;
    new vm.Script(m[2], { filename: file + "<script " + checked + ">" });
  }
  console.log(file + ": JS OK checked=" + checked + " skipped=" + skipped);
}
'@
	Invoke-NodeScript 'xadmin_content_template_js_check.js' $script
}

Invoke-Step 'capability display titles wiring' {
	$abilityTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_ability.html.tpl')
	$utf8 = [System.Text.Encoding]::UTF8
	$titleMarkers = @(
		("packTitles['content.slug'] = '" + $utf8.GetString([byte[]]@(0xe5,0x9b,0xba,0xe5,0xae,0x9a,0xe9,0x93,0xbe,0xe6,0x8e,0xa5)) + "'"),
		("packTitles['content.seo'] = '" + $utf8.GetString([byte[]]@(0xe6,0x90,0x9c,0xe7,0xb4,0xa2,0xe5,0xbc,0x95,0xe6,0x93,0x8e,0xe4,0xbc,0x98,0xe5,0x8c,0x96)) + " SEO'"),
		("packTitles['content.audit-log'] = '" + $utf8.GetString([byte[]]@(0xe6,0x93,0x8d,0xe4,0xbd,0x9c,0xe5,0xae,0xa1,0xe8,0xae,0xa1)) + "'"),
		("packTitles['content.import-export'] = '" + $utf8.GetString([byte[]]@(0xe5,0xaf,0xbc,0xe5,0x85,0xa5,0xe5,0xaf,0xbc,0xe5,0x87,0xba)) + "'")
	)
	foreach ($needle in $titleMarkers) {
		if ($abilityTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_ability.html.tpl missing capability display title marker: $needle"
		}
	}
	$mojibakePattern = ([char]0x9365).ToString() + '|' + ([char]0x95be).ToString() + '|' + ([char]0x70ac).ToString()
	if ($abilityTemplate -match $mojibakePattern) {
		throw 'managed_ability.html.tpl contains mojibake in capability titles'
	}
	Write-Output 'capability display titles wiring OK'
}

Invoke-Step 'managed dialog action wiring' {
	$abilityTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_ability.html.tpl')
	foreach ($needle in @(
		'.managed-ability-dialog .x-dialog-actions{border-top:1px solid #e6e6e6',
		'id="btnRedirectCancel_',
		'id="btnRedirectSave_',
		'id="btnMediaCancel_',
		'id="btnMediaSave_',
		'id="btnSeoCancel_',
		'id="btnSeoSave_'
	)) {
		if ($abilityTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_ability.html.tpl missing dialog action marker: $needle"
		}
	}
	$seoDialog = [regex]::Match($abilityTemplate, '(?s)function openSeoDialog\(row\).*?function runSeoPreview').Value
	if ([string]::IsNullOrWhiteSpace($seoDialog)) {
		throw 'managed_ability.html.tpl missing SEO dialog block'
	}
	foreach ($englishLabel in @('>Cancel</button>', '>Save</button>', 'Edit SEO Meta', 'Add SEO Meta')) {
		if ($seoDialog -match [regex]::Escape($englishLabel)) {
			throw "managed_ability.html.tpl SEO dialog still uses English action label: $englishLabel"
		}
	}
	$rowActions = [regex]::Match($abilityTemplate, '(?s)<script type="text/html" id="rowActions">.*?</script>').Value
	if ([string]::IsNullOrWhiteSpace($rowActions)) {
		throw 'managed_ability.html.tpl missing row action template'
	}
	foreach ($englishLabel in @('>Diff</a>', '>Restore</a>', '>Processed</a>', '>Download</a>')) {
		if ($rowActions -match [regex]::Escape($englishLabel)) {
			throw "managed_ability.html.tpl row action still uses English label: $englishLabel"
		}
	}
	foreach ($needle in @('renderRowActionResult', 'showRowActionResult', '/comment/status', '/static/task/retry', '/form/notification/replay')) {
		if ($abilityTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_ability.html.tpl missing row action result UI marker: $needle"
		}
	}
	Write-Output 'managed dialog action wiring OK'
}

Invoke-Step 'managed ability visible text localization' {
	$abilityTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_ability.html.tpl')
	$editorTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_editor.html.tpl')
	foreach ($needle in @(
		'Target Type',
		'Attachment XID',
		'Upload Attachment',
		'Revision Diff #',
		'changed fields',
		'No field changes',
		'Search index stats',
		'Sitemap cache metadata',
		'Form submission export',
		'Workflow action result',
		'Merge duplicate tags',
		'Merge Tags',
		'Preview Import',
		'Confirm Import',
		'Cleanup Logs',
		'Workflow actions are registered',
		'Add Field',
		'Remove Field',
		'Export Submissions',
		'Replay Failed',
		'Chunk Import',
		'Rebuild Rules',
		'Rebuild Index',
		'Refresh Entries',
		"title:'Assignee'",
		'Save Sort',
		'Load a topic content list'
	)) {
		if ($abilityTemplate -match [regex]::Escape($needle)) {
			throw "managed_ability.html.tpl still contains legacy English UI text: $needle"
		}
		if ($editorTemplate -match [regex]::Escape($needle)) {
			throw "managed_editor.html.tpl still contains legacy English UI text: $needle"
		}
	}
	Write-Output 'managed ability visible text localization OK'
}

Invoke-Step 'cms text mojibake scan' {
	$scanFiles = @(
		'docs/路由动态化与CMS能力包建设SPEC.md',
		'hosts/xadmin/data/content/templates/managed_ability.html.tpl',
		'hosts/xadmin/data/content/templates/managed_editor.html.tpl',
		'hosts/xadmin/data/content/templates/managed_category.html.tpl',
		'hosts/xadmin/data/content/templates/managed_dashboard.html.tpl',
		'hosts/xadmin/data/content/templates/managed_tasks.html.tpl',
		'hosts/xadmin/data/content/templates/managed_public.html.tpl'
	)
	$badChars = @(
		[string][char]0x6769,
		[string][char]0x741b,
		[string][char]0x93C4,
		[string][char]0x95C4,
		[string][char]0x6D93,
		[string][char]0x6D94,
		[string][char]0x8FA9,
		[string][char]0x721C
	)
	foreach ($relative in $scanFiles) {
		$path = Join-Path $Root $relative
		if (!(Test-Path $path)) { continue }
		$text = Get-Content -Raw -Encoding UTF8 $path
		foreach ($bad in $badChars) {
			if ($text.Contains($bad)) {
				throw "possible mojibake found in ${relative}: U+$(([int][char]$bad).ToString('X4'))"
			}
		}
	}
	Write-Output 'cms text mojibake scan OK'
}

Invoke-Step 'managed main template compile' {
	$src = Get-Content (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_main.c.tpl') -Raw
	$src = $src -replace '\{\{ABILITY_PACK_SCHEMA_SQL\}\}', ''
	$src = $src -replace '\{\{ABILITY_PACK_AUTH_REGISTRATIONS\}\}', ''
	$src = $src -replace '\{\{ABILITY_PACK_ROUTE_REGISTRATIONS\}\}', ''
	$src = $src -replace '\{\{ABILITY_PACK_MENU_REGISTRATIONS\}\}', ''
	$src = $src -replace '\{\{PLUGIN_XID\}\}', 'demo'
	$src = $src -replace '\{\{PLUGIN_TITLE_C\}\}', 'Demo'
	$src = $src -replace '\{\{PLUGIN_VERSION\}\}', '1.0.0'
	$tmp = Join-Path $env:TEMP 'xadmin_managed_main_check.c'
	$obj = Join-Path $env:TEMP 'xadmin_managed_main_check.o'
	$err = Join-Path $env:TEMP 'xadmin_managed_main_check.err'
	$enc = New-Object System.Text.UTF8Encoding($false)
	[System.IO.File]::WriteAllText($tmp, $src, $enc)
	& tcc -I (Join-Path $Root 'tcc/inc_xs') -I (Join-Path $Root 'tcc/include') -c $tmp -o $obj 2> $err
	$code = $LASTEXITCODE
	$warnings = if (Test-Path $err) { Get-Content $err -Raw } else { '' }
	if ($code -ne 0 -or ![string]::IsNullOrWhiteSpace($warnings)) {
		if ($warnings) { Write-Output $warnings }
		exit $(if ($code -ne 0) { $code } else { 1 })
	}
	Remove-Item -Force $tmp,$obj,$err -ErrorAction SilentlyContinue
	Write-Output 'managed_main template compile OK; warnings=0'
}

Invoke-Step 'host main syntax compile' {
	$obj = Join-Path $env:TEMP 'xadmin_host_main_check.o'
	$err = Join-Path $env:TEMP 'xadmin_host_main_check.err'
	& tcc -w -I (Join-Path $Root 'tcc/inc_xs') -I (Join-Path $Root 'tcc/include') -c (Join-Path $Root 'hosts/xadmin/script/main.c') -o $obj 2> $err
	$code = $LASTEXITCODE
	$errors = if (Test-Path $err) { Get-Content $err -Raw } else { '' }
	if ($code -ne 0) {
		if ($errors) { Write-Output $errors }
		exit $code
	}
	Remove-Item -Force $obj,$err -ErrorAction SilentlyContinue
	Write-Output 'host main syntax compile OK'
}

Invoke-Step 'declared capability source compile' {
	$packRoot = Join-Path $Root 'hosts/xadmin/capability-pack'
	$packDirs = Get-ChildItem -Path $packRoot -Directory | Sort-Object Name
	foreach ($dir in $packDirs) {
		$packPath = Join-Path $dir.FullName 'pack.json'
		if (!(Test-Path $packPath)) { continue }
		$pack = (Get-Content -Raw -Encoding UTF8 $packPath) | ConvertFrom-Json
		$sources = @($pack.sourceFiles) | Where-Object { ![string]::IsNullOrWhiteSpace([string]$_) }
		if ($sources.Count -eq 0) { continue }
		$includeDirs = @($pack.includeFiles) |
			Where-Object { ![string]::IsNullOrWhiteSpace([string]$_) } |
			ForEach-Object { Split-Path ([string]$_) -Parent } |
			Where-Object { ![string]::IsNullOrWhiteSpace([string]$_) } |
			Sort-Object -Unique
		foreach ($source in $sources) {
			$srcPath = Join-Path $dir.FullName ([string]$source)
			$obj = Join-Path $env:TEMP ("xadmin_" + $dir.Name.Replace('.', '_') + "_" + [System.IO.Path]::GetFileNameWithoutExtension([string]$source) + ".o")
			$err = Join-Path $env:TEMP ("xadmin_" + $dir.Name.Replace('.', '_') + "_" + [System.IO.Path]::GetFileNameWithoutExtension([string]$source) + ".err")
			$args = @('-I', (Join-Path $Root 'tcc/inc_xs'), '-I', (Join-Path $Root 'tcc/include'))
			foreach ($inc in $includeDirs) {
				$args += @('-I', (Join-Path $dir.FullName $inc))
			}
			$args += @('-c', $srcPath, '-o', $obj)
			& tcc @args 2> $err
			$code = $LASTEXITCODE
			$warnings = if (Test-Path $err) { Get-Content $err -Raw } else { '' }
			if ($code -ne 0 -or ![string]::IsNullOrWhiteSpace($warnings)) {
				if ($warnings) { Write-Output $warnings }
				exit $(if ($code -ne 0) { $code } else { 1 })
			}
			Remove-Item -Force $obj,$err -ErrorAction SilentlyContinue
			Write-Output "$($dir.Name): $source compile OK; warnings=0"
		}
	}
}

Invoke-Step 'capability source macro boundary' {
	$mainTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_main.c.tpl')
	$generator = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/script/content/content_generator.h')
	$generation = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/script/content/content_generation.h')
	$likePack = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/capability-pack/content.like/pack.json')
	$auditPack = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/capability-pack/content.audit-log/pack.json')
	$importExportPack = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/capability-pack/content.import-export/pack.json')
	foreach ($needle in @(
		'#ifdef XADMIN_CAP_CONTENT_SLUG',
		'#include "content_slug_pack.h"',
		'#ifdef XADMIN_CAP_CONTENT_LIKE',
		'#include "content_like_pack.h"',
		'#ifdef XADMIN_CAP_CONTENT_AUDIT_LOG',
		'#include "content_audit_log_pack.h"',
		'#ifdef XADMIN_CAP_CONTENT_IMPORT_EXPORT',
		'#include "content_import_export_pack.h"',
		'Managed_LinkDeclaredCapabilitySources',
		'XAdminContentSlugPackLinked',
		'XAdminContentLikePackLinked',
		'XAdminContentAuditLogPackLinked',
		'XAdminContentImportExportPackLinked'
	)) {
		if ($mainTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_main.c.tpl missing source macro boundary marker: $needle"
		}
	}
	foreach ($needle in @('Content_CapabilityDefineName', 'XADMIN_CAP_', '%s=1')) {
		if ($generator -notmatch [regex]::Escape($needle)) {
			throw "content_generator.h missing capability define marker: $needle"
		}
	}
	foreach ($needle in @(
		'if ( !bEnabled ) continue;',
		'Content_AppendDeclaredPackBuildPaths(tblPackDetail, &sExtraBuildSourcesJson, &sBuildIncludeDirsJson)',
		'Content_AppendDeclaredPackFiles(files, &iFileCount'
	)) {
		if ($generation -notmatch [regex]::Escape($needle)) {
			throw "content_generation.h missing enabled-only declared source marker: $needle"
		}
	}
	foreach ($needle in @('source/content_like_pack.c', 'include/content_like_pack.h')) {
		if ($likePack -notmatch [regex]::Escape($needle)) {
			throw "content.like pack.json missing declared source marker: $needle"
		}
	}
	foreach ($needle in @('source/content_audit_log_pack.c', 'include/content_audit_log_pack.h')) {
		if ($auditPack -notmatch [regex]::Escape($needle)) {
			throw "content.audit-log pack.json missing declared source marker: $needle"
		}
	}
	foreach ($needle in @('source/content_import_export_pack.c', 'include/content_import_export_pack.h')) {
		if ($importExportPack -notmatch [regex]::Escape($needle)) {
			throw "content.import-export pack.json missing declared source marker: $needle"
		}
	}
	Write-Output 'capability source macro boundary OK'
}

Invoke-Step 'capability hook slot contract wiring' {
	$generator = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/script/content/content_generator.h')
	$generation = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/script/content/content_generation.h')
	$spec = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'docs/CMS能力包收口执行SPEC.md')
	$capabilityDoc = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'docs/内容系统能力包规范.md')
	foreach ($needle in @(
		'Content_AppendCapabilityHookSlot',
		'Content_AppendCapabilityHookSlots',
		'capabilityHookSlots',
		'"schema"',
		'"route"',
		'"menu"',
		'"page"',
		'"task"',
		'"public-head"',
		'"public-render"',
		'xvoTableSetValue(tblRoot, "capabilityHookSlots", 19, arrHookSlots, TRUE)'
	)) {
		if ($generator -notmatch [regex]::Escape($needle)) {
			throw "content_generator.h missing capability hook slot marker: $needle"
		}
	}
	foreach ($needle in @(
		'Content_SpecHasCapability(tblSpecJson, "content.search")',
		'Content_SpecHasCapability(tblSpecJson, "content.form")',
		'Content_SpecHasCapability(tblSpecJson, "content.audit-log")'
	)) {
		if ($generation -notmatch [regex]::Escape($needle)) {
			throw "content_generation.h missing task dashboard ability marker: $needle"
		}
	}
	foreach ($needle in @('schema、route、menu、page、task、public-head、public-render', 'content.slug', 'content.like', 'disabled 不复制、不编译、不注册')) {
		if ($spec -notmatch [regex]::Escape($needle)) {
			throw "CMS ability closeout spec missing hook slot progress marker: $needle"
		}
	}
	foreach ($needle in @('capabilityHookSlots', 'schema', 'route', 'menu', 'page', 'task', 'public-head', 'public-render', 'content.category')) {
		if ($capabilityDoc -notmatch [regex]::Escape($needle)) {
			throw "content ability pack doc missing current hook/category boundary marker: $needle"
		}
	}
	Write-Output 'capability hook slot contract wiring OK'
}

Invoke-Step 'capability manifest generation wiring' {
	$generator = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/script/content/content_generator.h')
	$generation = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/script/content/content_generation.h')
	$capabilityEditor = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/wwwroot/content/js/editor-capability.js')
	$mainTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_main.c.tpl')
	foreach ($needle in @(
		'Content_BuildGeneratedCapabilityManifest',
		'loadPolicy',
		'enabled-packs-only',
		'manifestJson',
		'abilityPacks'
	)) {
		if ($generator -notmatch [regex]::Escape($needle)) {
			throw "content_generator.h missing capability manifest marker: $needle"
		}
	}
	foreach ($needle in @(
		'Content_BuildGeneratedCapabilityManifest',
		'runtime/capability.manifest.json'
	)) {
		if ($generation -notmatch [regex]::Escape($needle)) {
			throw "content_generation.h missing capability manifest wiring: $needle"
		}
	}
	foreach ($needle in @(
		'Content_BuildCapabilityDefinesJson',
		'if ( xvoTableExists(tblItem, "enabled", 7) ) bEnabled = xvoTableGetBool(tblItem, "enabled", 7)',
		'if ( !bEnabled ) continue',
		'Content_BuildAbilityPackSchemaSql',
		'Content_BuildAbilityPackRouteCode',
		'Content_BuildAbilityPackMenuCode',
		'Content_BuildAbilityPackAuthCode',
		'Content_BuildGeneratedCapabilityManifest',
		'arrEnabledCapabilities',
		'xvoArrayAppendValue(arrEnabledCapabilities, xvoCopy(tblItem), TRUE)',
		'xvoTableSetText(tblRoot, "loadPolicy", 10, "enabled-packs-only"',
		'Content_BuildGeneratedContracts'
	)) {
		if ($generator -notmatch [regex]::Escape($needle)) {
			throw "content_generator.h missing enabled-pack generation boundary marker: $needle"
		}
	}
	foreach ($needle in @(
		'Content_AppendDeclaredPackBuildPaths(tblPackDetail, &sExtraBuildSourcesJson, &sBuildIncludeDirsJson)',
		'Content_AppendDeclaredPackFiles(files, &iFileCount',
		'if ( xvoTableExists(tblCap, "enabled", 7) ) bEnabled = xvoTableGetBool(tblCap, "enabled", 7)',
		'if ( !bEnabled ) continue'
	)) {
		if ($generation -notmatch [regex]::Escape($needle)) {
			throw "content_generation.h missing enabled-pack declared-file boundary marker: $needle"
		}
	}
	foreach ($needle in @(
		'Content_BuildRuntimeManagedJson',
		'sManaged = Content_BuildRuntimeManagedJson',
		'xvalue arrEnabledCapabilities = xvoCreateArray()',
		'xvoTableSetText(tblRoot, "managedBy", 9, "content"',
		'xvoTableSetValue(tblRoot, "capabilitySlots", 15, arrEnabledCapabilities, TRUE)'
	)) {
		if ($generation -notmatch [regex]::Escape($needle)) {
			throw "content_generation.h missing runtime managed enabled-capability marker: $needle"
		}
	}
	$runtimeManagedBody = [regex]::Match($generation, '(?s)str Content_BuildRuntimeManagedJson\(.*?\n}\r?\n\r?\nxvalue Content_GeneratePluginForModel').Value
	if ([string]::IsNullOrWhiteSpace($runtimeManagedBody)) {
		throw 'content_generation.h missing Content_BuildRuntimeManagedJson body'
	}
	if ($runtimeManagedBody -notmatch [regex]::Escape('xvalue arrEnabledCapabilities = xvoCreateArray()') + '(?s).*' + [regex]::Escape('if ( arrCapabilities && xvoType(arrCapabilities) == XVO_DT_ARRAY )') + '(?s).*' + [regex]::Escape('xvoTableSetValue(tblRoot, "capabilitySlots", 15, arrEnabledCapabilities, TRUE)')) {
		throw 'content_generation.h must always emit runtime managed capabilitySlots, even when no packs are enabled'
	}
	if ($generation -match [regex]::Escape('sManaged = xrtFormat("{\"managed\":true')) {
		throw 'content_generation.h must not hand-build runtime/managed.json without capabilitySlots'
	}
	foreach ($needle in @(
		'renderPackConfigFromXForm',
		'renderXFormConfigField',
		'pack.instanceFormJson',
		"field.type || 'input'",
		'Array.isArray(form.groups)'
	)) {
		if ($capabilityEditor -notmatch [regex]::Escape($needle)) {
			throw "editor-capability.js missing xform config marker: $needle"
		}
	}
	foreach ($needle in @(
		'Managed_CapabilityMountMaxRequestBytes',
		'capability mount request body is too large',
		'Managed_BuildMountRegistry(xvalue tblManaged',
		'Managed_BuildProviderSuggestions(xvalue tblManaged',
		'Managed_BuildMountRegistry(tblManaged, tblCustomMounts)',
		'Managed_BuildProviderSuggestions(tblManaged, arrProviders)',
		'xvoTableGetValue(tblManaged, "capabilitySlots", 15)'
	)) {
		if ($mainTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_main.c.tpl missing capability mount boundary marker: $needle"
		}
	}
	foreach ($forbidden in @(
		'xvoTableGetValue(tblContracts, "capabilitySlots", 15)',
		'Managed_BuildMountRegistry(tblContracts, tblCustomMounts)',
		'Managed_BuildProviderSuggestions(tblContracts, arrProviders)',
		'contracts || {}).capabilitySlots',
		'managed || {}).contracts || {}).capabilitySlots'
	)) {
		if (($mainTemplate + "`n" + (Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_public.html.tpl'))) -match [regex]::Escape($forbidden)) {
			throw "managed templates must read capabilitySlots from managed meta, not contracts: $forbidden"
		}
	}
	if ((Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_public.html.tpl')) -notmatch [regex]::Escape('return ((((mp.meta || {}).managed || {}).capabilitySlots) || [])')) {
		throw 'managed_public.html.tpl must use managed.capabilitySlots as its slot fallback'
	}
	foreach ($relativePath in @(
		'hosts/xadmin/capability-pack/content.access/instance.xform.json',
		'hosts/xadmin/capability-pack/content.search/instance.xform.json',
		'hosts/xadmin/capability-pack/content.related/instance.xform.json',
		'hosts/xadmin/capability-pack/content.redirect/instance.xform.json',
		'hosts/xadmin/capability-pack/content.audit-log/instance.xform.json',
		'hosts/xadmin/capability-pack/content.workflow/instance.xform.json',
		'hosts/xadmin/capability-pack/content.import-export/instance.xform.json',
		'hosts/xadmin/capability-pack/content.slug/instance.xform.json',
		'hosts/xadmin/capability-pack/content.form/instance.xform.json',
		'hosts/xadmin/capability-pack/content.sitemap/instance.xform.json',
		'hosts/xadmin/capability-pack/content.seo/instance.xform.json',
		'hosts/xadmin/capability-pack/content.category/instance.xform.json',
		'hosts/xadmin/capability-pack/content.media/instance.xform.json',
		'hosts/xadmin/capability-pack/content.revision/instance.xform.json'
	)) {
		if (!(Test-Path (Join-Path $Root $relativePath))) {
			throw "missing capability xform file: $relativePath"
		}
	}
	foreach ($needle in @(
		'bool Content_SpecHasCapability(xvalue tblSpec, const char* sKey)',
		'objEnabled && (xvoType(objEnabled) == XVO_DT_BOOL)',
		'if ( bEnabled && sCapKey && (strcmp(sCapKey, sKey) == 0) )',
		'bCategoryPack = Content_SpecHasCapability(tblSpecJson, "content.category")',
		'bMetricPack = Content_SpecHasCapability(tblSpecJson, "content.like") || Content_SpecHasCapability(tblSpecJson, "content.view-stat")',
		'if ( bCategoryPack )',
		'Content_SetGeneratedFile(&files[iFileCount++], "generated/categories.html", sCategoryHtml)',
		'if ( bMetricPack )',
		'Content_SetGeneratedFile(&files[iFileCount++], "generated/dashboard.html", sDashboardHtml)'
	)) {
		if ($generation -notmatch [regex]::Escape($needle)) {
			throw "content_generation.h missing optional generated file boundary marker: $needle"
		}
	}
	$categoryFileWrites = [regex]::Matches($generation, 'Content_SetGeneratedFile\(&files\[iFileCount\+\+\], "generated/categories\.html"')
	$dashboardFileWrites = [regex]::Matches($generation, 'Content_SetGeneratedFile\(&files\[iFileCount\+\+\], "generated/dashboard\.html"')
	if ($categoryFileWrites.Count -ne 1) {
		throw "generated/categories.html must be written exactly once and only in the category pack branch"
	}
	if ($dashboardFileWrites.Count -ne 1) {
		throw "generated/dashboard.html must be written exactly once and only in the metric pack branch"
	}
	if ($generation -notmatch [regex]::Escape(': bCategoryPack') -or $generation -notmatch [regex]::Escape(': bMetricPack')) {
		throw 'content_generation.h output file list must keep category/dashboard conditional branches'
	}
	Write-Output 'capability manifest wiring OK'
}

Invoke-Step 'dynamic route risk warning wiring' {
	$route = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/script/module/define.h')
	$routeInit = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/script/route.h')
	$trace = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/script/route_http/trace.h')
	$httpModule = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/script/module/http.h')
	$pluginAbi = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/script/plugin_system/ps_abi.h')
	$authModule = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/script/module/auth.h')
	$authRoute = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/script/route_http/auth.h')
	$memberAuthModule = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/script/module/member_auth.h')
	$optionModule = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/script/module/option.h')
	$mainTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_main.c.tpl')
	$abilityTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_ability.html.tpl')
	$publicTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_public.html.tpl')
	$dynamicRouteSources = @($route, $pluginAbi, $mainTemplate) -join "`n"
	if ($dynamicRouteSources -match '(?i)\bbbre[_a-z0-9]*\b') {
		throw 'dynamic route implementation must use xrt regex API, not direct bbre API'
	}
	$scriptSources = Get-ChildItem -Path (Join-Path $Root 'hosts/xadmin/script') -Recurse -Include *.h,*.c
	foreach ($sourceFile in $scriptSources) {
		$sourceText = Get-Content -Raw -Encoding UTF8 $sourceFile.FullName
		if ($sourceText -match '(?i)\bbbre[_a-z0-9]*\b') {
			throw "xadmin script source must use xrt regex API, not direct bbre API: $($sourceFile.FullName)"
		}
	}
	foreach ($needle in @(
		'DynamicRoute_GetLastWarningHTTP',
		'DynamicRoute_RecordPatternRisk',
		'static asset overlap risk',
		'admin/API prefix overlap risk',
		'DynamicRoute_AppendWarning',
		'dynamic route candidate limit warning',
		'XADMIN_DYNAMIC_ROUTE_MAX_MATCHES',
		'XADMIN_DYNAMIC_ROUTE_MAX_CAPTURES',
		'arrMatches[XADMIN_DYNAMIC_ROUTE_MAX_MATCHES]',
		'arrCaptures[XADMIN_DYNAMIC_ROUTE_MAX_CAPTURES]',
		'iPatternIndex = arrMatches[i]',
		'DynamicRoute_SortMatchIndexes(arrMatches, iMatchCount)',
		'xrtRegexCreate(pattern)',
		'xrtRegexSetBuilderCreate',
		'xrtRegexSetMatches(G_DynamicRouteTableHTTP.pRegexSet, sPath, iPathLen, arrMatches, XADMIN_DYNAMIC_ROUTE_MAX_MATCHES',
		'dynamic route capture count exceeds route param limit',
		'xrtRegexCaptures(pRoute->pRegex, sPath, pCtx->iPathLen, pCtx->arrCaptures, pCtx->iCaptureCount) != 1',
		'iCaptureIndex = (uint32)iIndex + 1',
		'return (pCtx && pCtx->iCaptureCount > 0) ? (int)(pCtx->iCaptureCount - 1) : 0',
		'xrtRegexCaptures(pRoute->pRegex, sPath, iPathLen, span, 1) == 1',
		'DynamicRoute_RebuildHTTP()',
		'xregexset* pOldSet = G_DynamicRouteTableHTTP.pRegexSet',
		'iOldCompiledCount = G_DynamicRouteTableHTTP.iCompiledCount',
		'G_DynamicRouteTableHTTP.pRegexSet = pOldSet',
		'G_DynamicRouteTableHTTP.pRegexSet = pNewSet',
		'xrtRegexSetDestroy(pOldSet)',
		'DynamicRouteInfo* pRemoved = (DynamicRouteInfo*)xrtListRemovePtr',
		'xrtListSetPtr(G_DynamicRouteTableHTTP.lstRoutes, i, pRemoved, NULL)',
		'DynamicRoute_Free(pRemoved)'
	)) {
		if ($route -notmatch [regex]::Escape($needle)) {
			throw "define.h missing dynamic route warning marker: $needle"
		}
	}
	foreach ($forbidden in @(
		'xrtRegexCaptures(pRoute->pRegex, sPath, iPathLen, span, 1) == 0',
		'xrtRegexCaptures(pRoute->pRegex, sPath, pCtx->iPathLen, pCtx->arrCaptures, pCtx->iCaptureCount) != 0'
	)) {
		if ($route -match [regex]::Escape($forbidden)) {
			throw "define.h uses obsolete xrt regex capture success semantics: $forbidden"
		}
	}
	foreach ($needle in @(
		'xrtDictGet(G_StaticRouteTableHTTP, (str)sLookupPath, strlen(sLookupPath))',
		'MatchDynamicRouteHTTP(sPath, xsReqMethodID(objReq))',
		'PS_TryServePluginStatic(objReq, objResp, sPath)'
	)) {
		if ($httpModule -notmatch [regex]::Escape($needle)) {
			throw "http.h missing static/dynamic route dispatch marker: $needle"
		}
	}
	$staticIndex = $httpModule.IndexOf('xrtDictGet(G_StaticRouteTableHTTP, (str)sLookupPath, strlen(sLookupPath))')
	$dynamicIndex = $httpModule.IndexOf('MatchDynamicRouteHTTP(sPath, xsReqMethodID(objReq))')
	$pluginStaticIndex = $httpModule.IndexOf('PS_TryServePluginStatic(objReq, objResp, sPath)')
	if ($staticIndex -lt 0 -or $dynamicIndex -lt 0 -or $pluginStaticIndex -lt 0 -or !($staticIndex -lt $dynamicIndex -and $dynamicIndex -lt $pluginStaticIndex)) {
		throw 'http.h route dispatch order must be static route -> dynamic route -> plugin static'
	}
	foreach ($needle in @(
		'G_DynamicRouteInvokeContext = xrtDictCreate(sizeof(ptr), XRT_OBJMODE_SHARED)',
		'xrtOwnerActivateShared(&G_DynamicRouteInvokeContext->Owner)',
		'xrtOwnerActivateShared(&G_DynamicRouteInvokeContext->AVLT.Owner)'
	)) {
		if ($routeInit -notmatch [regex]::Escape($needle)) {
			throw "route.h missing dynamic route invoke context marker: $needle"
		}
	}
	foreach ($needle in @(
		'dynamicLastWarning',
		'G_DynamicRouteTableHTTP.sLastWarning',
		'dynamicCandidateLimit',
		'dynamicCandidateOverLimit',
		'G_DynamicRouteTableHTTP.iCompiledCount > XADMIN_DYNAMIC_ROUTE_MAX_MATCHES'
	)) {
		if ($trace -notmatch [regex]::Escape($needle)) {
			throw "trace.h missing dynamic route warning marker: $needle"
		}
	}
	foreach ($needle in @(
		'PS_HostApplyRouteToken',
		'AddDynamicRouteHTTPEx(pToken->sPath, pToken->sPattern, pToken->pProc, pToken->iPriority, pToken->iMethod)',
		'pInfo->bAuth = pToken->bNeedAuth',
		'pInfo->bAdmin = pToken->bAdminOnly',
		'pInfo->AuthID = iAuthId',
		'pInfo->AuthLevel = pToken->iAuthLevel',
		'DynamicRoute_BeginInvoke(pInfo, xsReqPath(objReq))',
		'DynamicRoute_EndInvoke()',
		'int XAdmin_RouteParam(int index, char* out_value, size_t out_cap)',
		'int XAdmin_RouteParamCount()'
	)) {
		if ($pluginAbi -notmatch [regex]::Escape($needle)) {
			throw "ps_abi.h missing dynamic route auth sync marker: $needle"
		}
	}
	foreach ($needle in @(
		'AuthWalkDynamicRoutes(AuthDynamicRouteCheckProc, NULL)',
		'AuthDynamicRouteCheckProc',
		'FindDynamicRouteHTTP(uri)'
	)) {
		if ($authModule -notmatch [regex]::Escape($needle)) {
			throw "auth.h missing dynamic route startup auth sync marker: $needle"
		}
	}
	foreach ($needle in @(
		'xrtDictGet(G_StaticRouteTableHTTP, uri, strlen(uri))',
		'FindDynamicRouteHTTP(uri)'
	)) {
		if ($authRoute -notmatch [regex]::Escape($needle)) {
			throw "auth.h missing dynamic route URI edit sync marker: $needle"
		}
	}
	foreach ($needle in @(
		'MemberAuth_LoadURIS',
		'xrtDictGet(G_StaticRouteTableHTTP, uri, iSize)',
		'FindDynamicRouteHTTP(uri)'
	)) {
		if ($memberAuthModule -notmatch [regex]::Escape($needle)) {
			throw "member_auth.h missing dynamic route member URI sync marker: $needle"
		}
	}
	foreach ($needle in @(
		'Option_AdminEntryConflictsRoute',
		'FindDynamicRouteHTTP((str)sPath)'
	)) {
		if ($optionModule -notmatch [regex]::Escape($needle)) {
			throw "option.h missing dynamic route admin entry conflict marker: $needle"
		}
	}
	foreach ($needle in @(
		'Managed_SlugBuildRiskWarning',
		'Managed_RedirectBuildRiskWarning',
		'Managed_StaticRuleBuildRiskWarning',
		'Managed_GetUiListMaxScanRows',
		'scanLimitReached',
		'ORDER BY %s LIMIT ?',
		'maxScanRows',
		'XAdmin_RegisterDynamicRoute(handle, &dynRoute',
		'Managed_SlugRoutePrefixDup',
		'Managed_RedirectRoutePrefixDup',
		'Managed_RegexEscapeLiteralDup',
		'xrtFormat("^%s/([^/]+)$"',
		'xrtFormat("^%s/[^?#]+$"',
		'Managed_AbilityPackConfigBool("content.redirect", "enablePrettyRedirectRoute", TRUE)',
		'content.redirect", "redirectRoutePrefix"',
		'Managed_RedirectMaxListRows',
		'Managed_ReadTextQuery(objReq, "sourcePath"',
		'Managed_ReadTextQuery(objReq, "targetUrl"',
		"content_redirect WHERE delete_time=0 AND (?='' OR source_path LIKE '%' || ? || '%') AND (?='' OR target_url LIKE '%' || ? || '%') AND (?=999 OR status=?)",
		'xvoTableSetText(tblRet, "sourcePathFilter"',
		'xvoTableSetText(tblRet, "targetUrlFilter"',
		'content.redirect", "maxImportRows"',
		'content.redirect", "maxRequestBytes"',
		'redirect import rows exceeded configured limit',
		'redirect request body is too large',
		'Managed_RequestRedirectDeleteAdmin',
		'redirect rule save failed',
		'redirect rule not found',
		'hitRecorded',
		'Managed_AbilityPackConfigBool("content.slug", "enablePrettySlugRoute", TRUE)',
		'content.slug", "slugRoutePrefix"',
		'content.slug", "maxHistoryRows"',
		'Managed_ReadTextQuery(objReq, "oldSlug"',
		'Managed_ReadTextQuery(objReq, "newSlug"',
		"content_slug_history WHERE (?<=0 OR content_id=?) AND (?='' OR old_slug LIKE '%' || ? || '%') AND (?='' OR new_slug LIKE '%' || ? || '%') AND (?=999 OR status=?)",
		'xvoTableSetText(tblRet, "oldSlugFilter"',
		'xvoTableSetText(tblRet, "newSlugFilter"',
		'content.slug", "maxPublicLookupRows"',
		'Managed_SlugMaxRepairRows',
		'content.slug", "maxRequestBytes"',
		'ALTER TABLE content_item ADD COLUMN slug_value',
		'idx_content_item_slug_value',
		'Managed_UpdateContentSlugValue',
		'SELECT id FROM content_item WHERE slug_value=?',
		'content_item SET payload_json=?,slug_value=?',
		'WHERE slug_value = ? AND delete_time = 0',
		'slug request body is too large',
		'slug repair transaction failed',
		'slug repair commit failed',
		'Managed_SyncRouteRuleSnapshot(pDb, iLimit, &iSyncedRouteRules)',
		'Managed_RefreshEditableRouteRulesRuntime(&iRuntimeRoutes, &sRouteRefreshError)',
		'xvoTableSetInt(tblData, "syncedRouteRules", 16, iSyncedRouteRules)',
		'xvoTableSetBool(tblData, "runtimeRefreshed", 16, bRuntimeRefreshed)',
		'xvoTableSetText(tblData, "routeRefreshError", 17, sRouteRefreshError',
		'slug lookup exceeded configured scan limit',
		'Managed_AccessCheckRule(pDb, xvoTableGetInt(tblCandidate, "id", 2), objReq, objSession, FALSE, NULL)',
		'Managed_AbilityPackConfigInt("content.import-export", "maxBatchRows", 200)',
		'Managed_AbilityPackConfigInt("content.audit-log", "defaultKeepDays", 90)',
		'Managed_AbilityPackConfigInt("content.workflow", "defaultScheduledLimit", 100)',
		'Managed_RedirectBuildLoopWarning',
		'redirect target forms a two-step loop with an existing rule',
		'redirect target forms a bounded chained loop with an existing rule',
		'redirect chain depth reaches submit-time check limit 8',
		'xsReqPath(objReq)',
		'static pathPattern covers site root',
		'static pathPattern overlaps admin/API prefix',
		'static pathPattern looks like a static resource path',
		'Managed_RequestRouteRulePlanAdmin',
		'/route-rule/plan',
		'Managed_RequestRouteRuleValidateAdmin',
		'/route-rule/validate',
		'Managed_RequestRouteRuleListAdmin',
		'/route-rule/list',
		'Managed_RequestRouteRuleSaveAdmin',
		'/route-rule/save',
		'Managed_RequestRouteRuleStatusAdmin',
		'/route-rule/status',
		'Managed_RequestRouteRuleSortAdmin',
		'/route-rule/sort',
		'Managed_RefreshEditableRouteRulesRuntime',
		'routeRefreshError',
		'runtimeRefreshed',
		'XAdmin_RouteParam(0, sSlug',
		'Managed_RequestRouteRuleStatsAdmin',
		'Managed_AppendRouteRuleGroupStat',
		'/route-rule/stats',
		'Managed_RequestRouteRuleRefreshAdmin',
		'/route-rule/refresh',
		'packStats',
		'ruleTypeStats',
		'Managed_RequestStaticRulePreviewAdmin',
		'/static/rule/preview',
		'Managed_StaticBuildRelPath(iTargetId, sExplicitPath, sPathPattern, tblItem)',
		'xvoTableSetText(tblData, "artifactUrl"',
		'xvoTableSetBool(tblData, "targetLoaded"',
		'Managed_AppendRouteRuleWarning',
		'Managed_AppendRoutePrefixConflictWarnings',
		'Managed_RouteRuleBuildSaveWarning',
		'Managed_RouteRuleHasStaticRouteConflict',
		'Managed_RouteRulePatternCompiles',
		'Managed_RouteRuleTypeValid',
		'Managed_AppendRouteRuleSnapshotRow',
		'G_RouteRuleSchemaSql',
		'content_route_rule',
		'source_pack TEXT NOT NULL DEFAULT',
		'match_pattern TEXT NOT NULL DEFAULT',
		'target_path TEXT NOT NULL DEFAULT',
		'priority INTEGER NOT NULL DEFAULT 0',
		'compile_status INTEGER NOT NULL DEFAULT 1',
		'compile_message TEXT NOT NULL DEFAULT',
		'managed_flag INTEGER NOT NULL DEFAULT 0',
		'idx_content_route_rule_key',
		'Managed_SyncRouteRuleSnapshot',
		'Managed_UpsertRouteRuleSnapshot',
		'Managed_CountRouteRuleSnapshot',
		'independentRouteRuleStore',
		'persistedRouteRules',
		'syncedRouteRules',
		'SELECT id,name,path_pattern,template_name,status FROM static_rule ORDER BY status DESC,id DESC LIMIT ?',
		'SELECT id,path_pattern FROM static_rule ORDER BY status DESC,id DESC LIMIT ?',
		'SELECT id,path_pattern,status FROM static_rule ORDER BY status DESC,id DESC LIMIT ?',
		'SELECT id,pack_id,rule_type,rule_key,source_pack,pattern,match_pattern,target_path,source,source_id,priority,status,warning,compile_status,compile_message,managed_flag,update_time FROM content_route_rule WHERE (?='''' OR pack_id=? OR source_pack=?)',
		"(?=0 OR warning<>'')",
		"(?='' OR rule_key LIKE '%'||?||'%' OR pattern LIKE '%'||?||'%' OR match_pattern LIKE '%'||?||'%' OR target_path LIKE '%'||?||'%' OR source LIKE '%'||?||'%' OR warning LIKE '%'||?||'%')",
		'Managed_ReadTextQuery(objReq, "sourcePack"',
		'Managed_ReadTextQuery(objReq, "keyword"',
		'xvoTableSetText(tblRow, "sourcePack"',
		'xvoTableSetText(tblRow, "matchPattern"',
		'xvoTableSetText(tblRow, "targetPath"',
		'xvoTableSetInt(tblRow, "priority"',
		'xvoTableSetInt(tblRow, "compileStatus"',
		'xvoTableSetText(tblRow, "compileMessage"',
		'xvoTableSetInt(tblRow, "managedFlag"',
		'route-rule.create',
		'route-rule.update',
		'route-rule.status',
		'route-rule.sort',
		'updatedCount',
		'persistedStaticRules',
		'checkedStaticRules',
		'adminTimeOnly',
		'"warning"'
	)) {
		if ($mainTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_main.c.tpl missing route risk warning marker: $needle"
		}
	}
	if ($mainTemplate -notmatch [regex]::Escape('bMatch = xrtRegexCaptures(pRegex, sText, iLen, span, 1) == 1 && span[0].iBegin == 0 && span[0].iEnd == iLen')) {
		throw 'managed_main.c.tpl missing xrt regex capture success marker for form pattern validation'
	}
	if ($mainTemplate -match [regex]::Escape('bMatch = xrtRegexCaptures(pRegex, sText, iLen, span, 1) == 0')) {
		throw 'managed_main.c.tpl uses obsolete xrt regex capture success semantics for form pattern validation'
	}
	$slugContracts = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/capability-pack/content.slug/contracts.json')
	foreach ($needle in @('accessIntegration', 'categoryBindIntegration', 'submitWarnings', 'checkPreviewUi', 'repairResultUi', 'repairRouteRuleRefresh', 'repairRouteRuleRefreshUi', 'routeRulePreview', 'ruleConflictExplain', 'crossAbilityRuleCheck', 'unifiedRulePlan', 'routeRulePlanApi', 'routeRuleValidateApi', 'routeRuleListApi', 'routeRuleSaveApi', 'routeRuleStatusApi', 'routeRuleSortApi', 'route-rule.save', 'routeRuleStatsApi', 'routeRuleRefreshApi', 'routeRulePlanUi', 'routeRuleValidateUi', 'routeRuleStatsUi', 'routeRuleRefreshUi', 'routeRuleEditDialogUi', 'routeRuleRuntimeRefresh', 'independentRouteRuleStore', 'routeRuleEditableContract', 'routeRuleListFilters', '"sourcePack"', '"keyword"', 'historyFilters')) {
		if ($slugContracts -notmatch [regex]::Escape($needle)) {
			throw "content.slug contracts.json missing access integration marker: $needle"
		}
	}
	foreach ($needle in @(
		'mpPrettySlugFromPath',
		'mpAbilityRoutePrefix("content.slug", "slugRoutePrefix", "/{{PLUGIN_XID}}")',
		'params.get("slug") || mpPrettySlugFromPath()',
		'renderSlugCheckResult',
		'renderSlugRepairResult',
		'syncedRouteRules',
		'runtimeRefreshed',
		'routeRefreshError',
		'renderSlugRepairRows',
		'canonicalUrl',
		'conflictId',
		'maxRepairRows',
		'ret.warning',
		'ret.data && ret.data.warning',
		'redirectImportResult',
		'btnSlugRuleExplain',
		'btnSlugHistoryFilter',
		'btnSlugHistoryFilterClear',
		'slugHistoryContentId',
		'slugHistoryOldSlug',
		'slugHistoryNewSlug',
		'slugHistoryStatus',
		"['oldSlug', abilityListFilters.slugHistoryOldSlug]",
		"['newSlug', abilityListFilters.slugHistoryNewSlug]",
		'btnUnifiedRouteRuleExplain',
		'btnUnifiedRouteRuleValidate',
		'btnRouteRuleFilter',
		'btnRouteRuleFilterClear',
		'btnRouteRuleRefresh',
		'btnAddRouteRule',
		'openRouteRuleDialog',
		'btnRouteRuleSave_',
		'routeRuleDialogWarning_',
		'routeRuleLocalWarning',
		'routeRuleEdit',
		'routeRuleToggle',
		'routeRuleSort',
		"api('/route-rule/status')",
		"api('/route-rule/sort')",
		"field:'sourcePack'",
		"field:'matchPattern'",
		"field:'targetPath'",
		"field:'priority'",
		"field:'compileStatus'",
		"field:'compileMessage'",
		"field:'managedFlag'",
		'btnRouteRuleStats',
		'showRouteRuleStats',
		'renderRouteRulePlan',
		'renderRouteRuleValidation',
		'renderRouteRuleStats',
		'renderRouteRuleRefreshResult',
		'runRouteRuleRefresh',
		'btnStaticRulePreview',
		'staticRulePreviewTargetId',
		'staticRulePreviewResult',
		'runStaticRulePreview',
		"api('/static/rule/preview')",
		'routeRulePackId',
		'routeRuleType',
		'routeRuleStatus',
		'routeRuleWarningOnly',
		"['packId', abilityListFilters.routeRulePackId]",
		"['warningOnly', abilityListFilters.routeRuleWarningOnly]",
		'runSlugRuleExplain',
		'btnRedirectRuleExplain',
		'btnRedirectUnifiedRuleExplain',
		'btnRedirectUnifiedRuleValidate',
		'btnRedirectFilter',
		'btnRedirectFilterClear',
		'redirectSourcePathFilter',
		'redirectTargetUrlFilter',
		'redirectStatusFilter',
		"['sourcePath', abilityListFilters.redirectSourcePath]",
		"['targetUrl', abilityListFilters.redirectTargetUrl]",
		'runRedirectRuleExplain',
		'runUnifiedRouteRuleExplain',
		'runUnifiedRouteRuleValidate',
		"api('/route-rule/plan')",
		"api('/route-rule/validate')",
		"api('/route-rule/stats')",
		"api('/route-rule/refresh')",
		"listApi:'/route-rule/list'",
		'routeRules',
		'loadAbilityPackContract',
		'unifiedRulePlan',
		'packsMounted',
		'staticRule',
		'content.static',
		'static_rule.pathPattern',
		'routePrefixConflictWarnings',
		'hotPathPolicy',
		'crossAbilityRuleCheck',
		'routeRulePreview',
		'ruleConflictExplain'
	)) {
		if (($abilityTemplate -notmatch [regex]::Escape($needle)) -and ($publicTemplate -notmatch [regex]::Escape($needle))) {
			throw "managed ability/public template missing route marker: $needle"
		}
	}
	$redirectContracts = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/capability-pack/content.redirect/contracts.json')
	foreach ($needle in @('boundedChainedLoop', 'chainDepthLimit8', 'redirectRoutePrefix', 'routePrefixConfig', 'maxListRows', 'maxImportRows', 'maxRequestBytes', 'importLimit', 'requestLimit', 'importResultUi', 'routeRulePreview', 'ruleConflictExplain', 'crossAbilityRuleCheck', 'unifiedRulePlan', 'routeRulePlanApi', 'routeRuleValidateApi', 'routeRuleListApi', 'routeRuleSaveApi', 'routeRuleStatusApi', 'routeRuleSortApi', 'routeRuleStatsApi', 'routeRuleRefreshApi', 'routeRulePlanUi', 'routeRuleValidateUi', 'routeRuleStatsUi', 'routeRuleRefreshUi', 'routeRuleEditDialogUi', 'routeRuleRuntimeRefresh', 'redirectSaveRouteRuleRefresh', 'redirectImportRouteRuleRefresh', 'redirectImportRouteRuleRefreshUi', 'route-rule.validate', 'route-rule.list', 'route-rule.save', 'route-rule.status', 'route-rule.sort', 'route-rule.stats', 'route-rule.refresh', 'independentRouteRuleStore', 'routeRuleEditableContract', 'routeRuleListFilters', '"sourcePack"', '"keyword"', 'redirectListFilters')) {
		if ($redirectContracts -notmatch [regex]::Escape($needle)) {
			throw "content.redirect contracts.json missing submit warning marker: $needle"
		}
	}
	$redirectPack = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/capability-pack/content.redirect/pack.json')
	if ($redirectPack -notmatch [regex]::Escape('/redirect/list')) {
		throw 'content.redirect pack.json acceptanceApiPath must use bounded redirect/list route, not public resolve'
	}
	foreach ($needle in @('renderRedirectImportResult', 'renderRedirectImportRows', 'redirectImportResult', 'redirectSaveResult_', 'syncedRouteRules', 'runtimeRefreshed', 'runtimeRouteCount', 'routeRefreshError', 'rowIndex', 'sourcePath', 'targetUrl')) {
		if ($abilityTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_ability.html.tpl missing redirect import UI marker: $needle"
		}
	}
	foreach ($needle in @('Managed_SyncRouteRuleSnapshot(pDb, Managed_RedirectMaxListRows(), &iSyncedRouteRules)', 'Managed_RefreshEditableRouteRulesRuntime(&iRuntimeRoutes, &sRouteRefreshError)', '"syncedRouteRules"', '"runtimeRefreshed"', '"runtimeRouteCount"', '"routeRefreshError"')) {
		if ($mainTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_main.c.tpl missing redirect route refresh marker: $needle"
		}
	}
	if ($redirectPack -match [regex]::Escape('/redirect/resolve')) {
		throw 'content.redirect pack.json acceptanceApiPath must not use resolve route because it updates hit stats'
	}
	$slugContracts = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/capability-pack/content.slug/contracts.json')
	foreach ($needle in @('slugRoutePrefix', 'routePrefixConfig', 'maxRepairRows', 'maxRequestBytes', 'repairLimit', 'requestLimit', 'physicalSlugColumn', 'content_item.slug_value', 'categoryBindIntegration')) {
		if ($slugContracts -notmatch [regex]::Escape($needle)) {
			throw "content.slug contracts.json missing slug boundary marker: $needle"
		}
	}
	Write-Output 'dynamic route risk warning wiring OK'
}

Invoke-Step 'managed multi-row limit guard' {
	$mainPath = Join-Path $Root 'hosts/xadmin/data/content/templates/managed_main.c.tpl'
	$mainTemplate = Get-Content -Raw -Encoding UTF8 $mainPath
	$lines = Get-Content -Encoding UTF8 $mainPath
	$violations = @()
	for ($i = 0; $i -lt $lines.Count; $i++) {
		$line = $lines[$i]
		if ($line -match 'LIMIT\s+([2-9]|[1-9][0-9]+)') {
			if ($line -match 'redirect chain depth reaches submit-time check limit 8') {
				continue
			}
			$violations += ("{0}:{1}: {2}" -f $mainPath, ($i + 1), $line.Trim())
		}
	}
	if ($violations.Count -gt 0) {
		throw ("managed_main.c.tpl has hard-coded multi-row LIMIT values; use ability-pack config + bound LIMIT instead:`n" + ($violations -join "`n"))
	}
	foreach ($needle in @(
		'Managed_ContentMaxRequestBytes',
		'content request body is too large',
		'bool bCategoryPack = FALSE',
		'bCategoryPack = Managed_AbilityPackMounted("content.category")',
		'Managed_RowMatchesFilters(tblItem, tblSpec, objReq, sQuery, bAdmin, bCategoryPack',
		'if ( bCategoryPack && (sCategoryId[0] != ''\0'') )'
	)) {
		if ($mainTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_main.c.tpl missing base content request boundary marker: $needle"
		}
	}
	$baseSchema = [regex]::Match($mainTemplate, '(?s)static const char\* G_SchemaSql =.*?;\s*static const char\* G_CategoryPostMigrationIndexSql').Value
	if ($baseSchema -match 'CREATE TABLE IF NOT EXISTS content_category') {
		throw 'base G_SchemaSql must not create content_category; it belongs to content.category/schema.sql'
	}
	$packRoot = Join-Path $Root 'hosts/xadmin/capability-pack'
	foreach ($contractsPath in Get-ChildItem -Path $packRoot -Directory | ForEach-Object { Join-Path $_.FullName 'contracts.json' }) {
		if (!(Test-Path $contractsPath)) { continue }
		$contracts = Get-Content -Raw -Encoding UTF8 $contractsPath | ConvertFrom-Json
		foreach ($table in @($contracts.tables)) {
			$sTable = [string]$table
			if ([string]::IsNullOrWhiteSpace($sTable)) { continue }
			if ($baseSchema -match [regex]::Escape("CREATE TABLE IF NOT EXISTS $sTable")) {
				throw "base G_SchemaSql must not create capability table '$sTable'; move it to the owning pack schema.sql"
			}
		}
	}
	Write-Output 'managed multi-row limit guard OK'
}

Invoke-Step 'background task schema wiring' {
	$mainTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_main.c.tpl')
	foreach ($needle in @(
		'G_BackgroundTaskSchemaSql',
		'CREATE TABLE IF NOT EXISTS content_background_task',
		'Managed_RequestTaskCreateAdmin',
		'Managed_RequestTaskListAdmin',
		'Managed_RequestTaskDetailAdmin',
		'Managed_RequestTaskCancelAdmin',
		'Managed_RequestTaskRetryAdmin',
		'Managed_RequestTasksView',
		'Managed_BackgroundTaskCreate',
		'content.static.generate',
		'content.static.retryFailed',
		'content.sitemap.refresh',
		'content.search.rebuild',
		'content.form.notification.deliver',
		'content.form.notification.replay',
		'content.import.stage',
		'content.import.process',
		'content.export.process',
		'content.audit.cleanup',
		'backgroundTaskId',
		'queued',
		'/task/create',
		'/task/list',
		'/task/detail',
		'/task/cancel',
		'/task/retry',
		'/admin/view/plugin/{{PLUGIN_XID}}/tasks',
		'generated/tasks.html',
		'Managed content task dashboard',
		'task_type TEXT NOT NULL DEFAULT',
		'target_type TEXT NOT NULL DEFAULT',
		'target_id INTEGER NOT NULL DEFAULT 0',
		'progress INTEGER NOT NULL DEFAULT 0',
		'payload_json TEXT NOT NULL DEFAULT',
		'result_json TEXT NOT NULL DEFAULT',
		'error_message TEXT NOT NULL DEFAULT',
		'retry_count INTEGER NOT NULL DEFAULT 0',
		'idx_content_background_task_status',
		'idx_content_background_task_target',
		'Managed_AbilityPackMounted("content.static") || Managed_AbilityPackMounted("content.sitemap") || Managed_AbilityPackMounted("content.import-export")'
	)) {
		if ($mainTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_main.c.tpl missing background task schema marker: $needle"
		}
	}
	foreach ($packId in @('content.static', 'content.sitemap', 'content.import-export')) {
		$contracts = Get-Content -Raw -Encoding UTF8 (Join-Path $Root "hosts/xadmin/capability-pack/$packId/contracts.json")
		foreach ($needle in @('content_background_task', 'backgroundTaskSchema', 'backgroundTaskApis')) {
			if ($contracts -notmatch [regex]::Escape($needle)) {
				throw "$packId contracts.json missing background task marker: $needle"
			}
		}
	}
	$staticContracts = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/capability-pack/content.static/contracts.json')
	foreach ($needle in @('backgroundTaskStaticGenerate', 'backgroundTaskStaticRetryFailed')) {
		if ($staticContracts -notmatch [regex]::Escape($needle)) {
			throw "content.static contracts.json missing background static task marker: $needle"
		}
	}
	$sitemapContracts = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/capability-pack/content.sitemap/contracts.json')
	if ($sitemapContracts -notmatch [regex]::Escape('backgroundTaskSitemapRefresh')) {
		throw 'content.sitemap contracts.json missing background sitemap refresh marker'
	}
	$importExportContracts = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/capability-pack/content.import-export/contracts.json')
	foreach ($needle in @('backgroundTaskImportStage', 'backgroundTaskImportProcess', 'backgroundTaskExportProcess')) {
		if ($importExportContracts -notmatch [regex]::Escape($needle)) {
			throw "content.import-export contracts.json missing background import/export task marker: $needle"
		}
	}
	$taskTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_tasks.html.tpl')
	$generator = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/script/content/content_generator.h')
	$generation = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/script/content/content_generation.h')
	foreach ($needle in @('managed_tasks.html.tpl', 'Content_BuildManagedTasksHtml', 'generated/tasks.html', 'bTaskPack')) {
		if (($generator + $generation) -notmatch [regex]::Escape($needle)) {
			throw "content generator missing background task page marker: $needle"
		}
	}
	foreach ($needle in @('TaskTable_{{PLUGIN_DOM_ID_BASE}}', '/task/list', '/task/detail', '/task/retry', '/task/cancel', '任务详情', 'task-actions')) {
		if ($taskTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_tasks.html.tpl missing background task page marker: $needle"
		}
	}
	Write-Output 'background task schema wiring OK'
}

Invoke-Step 'managed request safety scans' {
	$script = @'
const fs = require("fs");
const file = "hosts/xadmin/data/content/templates/managed_main.c.tpl";
const source = fs.readFileSync(file, "utf8");
const fnRe = /void\s+(Managed_Request\w*(?:Admin|Public))\s*\([^)]*objReq[^)]*\)\s*\{/g;

function bodyAt(index) {
  const start = source.indexOf("{", index);
  let depth = 0;
  for (let i = start; i < source.length; i++) {
    if (source[i] === "{") depth++;
    else if (source[i] === "}" && --depth === 0) return source.slice(start, i + 1);
  }
  return "";
}

function lineOf(index) {
  return source.slice(0, index).split(/\r?\n/).length;
}

const jsonGuardViolations = [];
const publicAccessViolations = [];
const limitConfigViolations = [];
let match;
while ((match = fnRe.exec(source))) {
  const name = match[1];
  const body = bodyAt(match.index);
  const where = `${file}:${lineOf(match.index)}:${name}`;
  if (/Managed_ParseJsonBody\(objReq\)|xrtParseJSON\(\(str\)xsReqBody\(objReq\), xsReqBodyLen\(objReq\)\)/.test(body) && !/xsReqBodyLen\(objReq\)\s*>/.test(body)) {
    jsonGuardViolations.push(where);
  }
  if (/Public$/.test(name) && /contentId|content_id|contentTitle|view_count|like_count|contentCount/.test(body) && !/Managed_AccessCheckRule|Managed_PublicContentVisibleForAccess|Managed_MaskPublicCountForAccess|Managed_CategoryMaskPublicCountForAccess|Managed_AccessReferencedMediaVisible/.test(body)) {
    publicAccessViolations.push(where);
  }
  if (/ORDER BY[^"]*LIMIT \?/.test(body) && !/Managed_AbilityPackConfigInt|Managed_ReadIntQuery|Managed_CommentMaxAdminListRows|Managed_RedirectMaxListRows|Managed_SlugMaxRepairRows|Managed_SitemapRefreshLimitFromRequest/.test(body)) {
    limitConfigViolations.push(where);
  }
}

const failures = [];
if (jsonGuardViolations.length) failures.push("JSON body parse without xsReqBodyLen guard:\n" + jsonGuardViolations.join("\n"));
if (publicAccessViolations.length) failures.push("public content exposure without access/mask marker:\n" + publicAccessViolations.join("\n"));
if (limitConfigViolations.length) failures.push("bound LIMIT request without query/config limit marker:\n" + limitConfigViolations.join("\n"));
if (failures.length) {
  console.error(failures.join("\n\n"));
  process.exit(1);
}
console.log("managed request safety scans OK");
'@
	Invoke-NodeScript 'xadmin_managed_request_safety_scans.js' $script
}

Invoke-Step 'base content write guard' {
	$mainTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_main.c.tpl')
	$saveBody = [regex]::Match($mainTemplate, '(?s)void Managed_RequestSave\(.*?\n\}\r?\n\r?\nvoid Managed_RequestDelete').Value
	if ([string]::IsNullOrWhiteSpace($saveBody)) {
		throw 'managed_main.c.tpl missing content save body for write guard check'
	}
	foreach ($needle in @(
		'bSaved = sqlite3_changes(pDb) > 0 ? TRUE : FALSE',
		'bSaved = iId > 0 ? TRUE : FALSE',
		'content save failed',
		'content not found or deleted',
		'!Managed_SlugHistoryInsert(pDb, iId',
		'!Managed_SlugRedirectSync(pDb, iId',
		'!Managed_RevisionSnapshot(pDb, iId',
		'content side effect failed',
		'if ( !Managed_AbilityPackMounted("content.category") )',
		'iCategoryId = 0',
		'Managed_RevisionSnapshot(pDb, iId',
		'Managed_ContentSyncDerivedData(pDb, tblSpec, iId',
		'content derived sync failed',
		'Managed_StaticMaybeAutoGenerate(pDb, iId'
	)) {
		if ($saveBody -notmatch [regex]::Escape($needle)) {
			throw "managed_main.c.tpl content save missing write guard marker: $needle"
		}
	}
	$deleteBody = [regex]::Match($mainTemplate, '(?s)void Managed_RequestDelete\(.*?\n\}\r?\n\r?\nvoid Managed_AppendCategoryRow').Value
	if ([string]::IsNullOrWhiteSpace($deleteBody)) {
		throw 'managed_main.c.tpl missing content delete body for write guard check'
	}
	foreach ($needle in @(
		'bDeleted = sqlite3_changes(pDb) > 0 ? TRUE : FALSE',
		'content not found or deleted',
		'Managed_AuditLogWithRequest(pDb, "content", iId, "content.delete"',
		'!Managed_SeoDelete(pDb, iId)',
		'!Managed_SearchIndexDelete(pDb, iId)',
		'!Managed_SitemapRemoveEntry(pDb, iId)',
		'!Managed_StaticMaybeAutoClean(pDb, iId)',
		'content derived delete failed',
		'Managed_StaticMaybeAutoClean(pDb, iId)'
	)) {
		if ($deleteBody -notmatch [regex]::Escape($needle)) {
			throw "managed_main.c.tpl content delete missing write guard marker: $needle"
		}
	}
	Write-Output 'base content write guard OK'
}

Invoke-Step 'last insert id guard scan' {
	$script = @'
const fs = require("fs");
const file = "hosts/xadmin/data/content/templates/managed_main.c.tpl";
const lines = fs.readFileSync(file, "utf8").split(/\r?\n/);
const failures = [];
for (let i = 0; i < lines.length; i++) {
  if (!/sqlite3_last_insert_rowid\s*\(/.test(lines[i])) continue;
  const window = lines.slice(Math.max(0, i - 4), i + 1).join("\n");
  if (!/sqlite3_step\s*\(\s*stmt\s*\)\s*==\s*SQLITE_DONE/.test(window) && !/bTaskOK\s*=\s*\(\s*sqlite3_step\s*\(\s*stmt\s*\)\s*==\s*SQLITE_DONE\s*\)/.test(window)) {
    failures.push(`${file}:${i + 1}: sqlite3_last_insert_rowid must be guarded by SQLITE_DONE`);
  }
}
if (failures.length) {
  console.error(failures.join("\n"));
  process.exit(1);
}
console.log("last insert id guard scan OK");
'@
	Invoke-NodeScript 'xadmin_last_insert_id_guard_scan.js' $script
}

Invoke-Step 'managed sqlite write result scan' {
	$mainTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_main.c.tpl')
	if ($mainTemplate -match 'sqlite3_step\([^)]*\);') {
		throw 'managed_main.c.tpl contains unchecked sqlite3_step call'
	}
	Write-Output 'managed sqlite write result scan OK'
}

Invoke-Step 'managed transaction rollback scan' {
	$script = @'
const fs = require("fs");
const file = "hosts/xadmin/data/content/templates/managed_main.c.tpl";
const lines = fs.readFileSync(file, "utf8").split(/\r?\n/);
const failures = [];

function blockWindow(start) {
  let depth = 0;
  const out = [];
  for (let i = start; i < lines.length && out.length < 32; i++) {
    const line = lines[i];
    out.push(line);
    for (const ch of line) {
      if (ch === "{") depth++;
      if (ch === "}") depth--;
    }
    if (i > start && depth <= 0) break;
  }
  return out.join("\n");
}

for (let i = 0; i < lines.length; i++) {
  const line = lines[i];
  if (!/sqlite3_exec\s*\(\s*pDb\s*,\s*"COMMIT"/.test(line)) continue;
  if (!/!=\s*SQLITE_OK/.test(line)) continue;
  const body = blockWindow(i);
  if (!/sqlite3_exec\s*\(\s*pDb\s*,\s*"ROLLBACK"/.test(body)) {
    failures.push(`${file}:${i + 1}: COMMIT failure branch must explicitly ROLLBACK`);
  }
}

if (failures.length) {
  console.error(failures.join("\n"));
  process.exit(1);
}
console.log("managed transaction rollback scan OK");
'@
	Invoke-NodeScript 'xadmin_managed_transaction_rollback_scan.js' $script
}

Invoke-Step 'managed route capability boundary scan' {
	$script = @'
const fs = require("fs");
const file = "hosts/xadmin/data/content/templates/managed_main.c.tpl";
const lines = fs.readFileSync(file, "utf8").split(/\r?\n/);
const stack = [];
const routes = [];
let currentPath = null;
let currentDynPath = null;

function countChar(text, ch) {
  let n = 0;
  for (const c of text) if (c === ch) n++;
  return n;
}

for (let i = 0; i < lines.length; i++) {
  const line = lines[i];
  const leadingClose = (line.match(/^\s*}+/) || [""])[0].replace(/\s/g, "").length;
  for (let k = 0; k < leadingClose; k++) stack.pop();

  const ifMatch = line.match(/^\s*if \((.+)\) \{/);
  if (ifMatch) {
    stack.push(ifMatch[1].trim());
  } else {
    const opens = countChar(line, "{");
    const closes = countChar(line, "}") - leadingClose;
    for (let k = 0; k < opens; k++) stack.push("");
    for (let k = 0; k < closes; k++) stack.pop();
  }

  const pathMatch = line.match(/route\.path = "([^"]+)";/);
  if (pathMatch) currentPath = pathMatch[1];
  const procMatch = line.match(/route\.proc = (Managed_Request\w+);/);
  if (procMatch && currentPath) {
    routes.push({ line: i + 1, path: currentPath, proc: procMatch[1], guards: stack.filter(Boolean) });
    currentPath = null;
  }
  const dynPathMatch = line.match(/dynRoute\.path = "([^"]+)";/);
  if (dynPathMatch) currentDynPath = dynPathMatch[1];
  const dynProcMatch = line.match(/dynRoute\.proc = (Managed_Request\w+);/);
  if (dynProcMatch && currentDynPath) {
    routes.push({ line: i + 1, path: currentDynPath, proc: dynProcMatch[1], guards: stack.filter(Boolean), dynamic: true });
    currentDynPath = null;
  }

  if (ifMatch) {
    const rest = line.slice(line.indexOf("{") + 1);
    const opens = countChar(rest, "{");
    const closes = countChar(rest, "}");
    for (let k = 0; k < opens; k++) stack.push("");
    for (let k = 0; k < closes; k++) stack.pop();
  }
}

const required = [
  [/\/category(\/|$)|\/categories$/, "bCategoryPack"],
  [/\/slug\//, "bSlugPack"],
  [/\/seo\//, "bSeoPack"],
  [/\/redirect\//, "bRedirectPack"],
  [/\/media\//, "bMediaPack"],
  [/\/revision\//, "bRevisionPack"],
  [/\/workflow\//, "bWorkflowPack"],
  [/\/search($|\/)/, "bSearchPack"],
  [/\/sitemap($|\/)|sitemap\.xml|sitemap-index\.xml|rss\.xml|robots\.txt/, "bSitemapPack"],
  [/\/related\//, "bRelatedPack"],
  [/\/form($|\/)/, "bFormPack"],
  [/\/access\//, "bAccessPack"],
  [/\/comment\//, "bCommentPack"],
  [/\/audit-log\//, "bAuditLogPack"],
  [/\/import-export\//, "bImportExportPack"],
  [/\/tag\//, "bTagPack"],
  [/\/topic\//, "bTopicPack"],
  [/\/sensitive\//, "bSensitivePack"],
  [/\/static\//, "bStaticPack"],
  [/\/like\//, "bLikePack"],
  [/\/view\//, "bViewPack"]
];
const basePaths = new Set([
  "/api/plugin/{{PLUGIN_XID}}/meta",
  "/api/plugin/{{PLUGIN_XID}}/list",
  "/api/plugin/{{PLUGIN_XID}}/detail",
  "/api/plugin/{{PLUGIN_XID}}/contracts",
  "/plugin/{{PLUGIN_XID}}",
  "/admin/api/plugin/{{PLUGIN_XID}}/list",
  "/admin/api/plugin/{{PLUGIN_XID}}/drafts",
  "/admin/api/plugin/{{PLUGIN_XID}}/contracts",
  "/admin/api/plugin/{{PLUGIN_XID}}/pack/meta",
  "/admin/api/plugin/{{PLUGIN_XID}}/pack/list",
  "/admin/api/plugin/{{PLUGIN_XID}}/form-meta",
  "/admin/api/plugin/{{PLUGIN_XID}}/get",
  "/admin/api/plugin/{{PLUGIN_XID}}/save",
  "/admin/api/plugin/{{PLUGIN_XID}}/delete",
  "/admin/view/plugin/{{PLUGIN_XID}}",
  "/admin/view/plugin/{{PLUGIN_XID}}/articles",
  "/admin/view/plugin/{{PLUGIN_XID}}/drafts",
  "/admin/view/plugin/{{PLUGIN_XID}}/editor"
]);
const failures = [];
for (const route of routes) {
  const guardText = route.guards.join(" && ");
  if (route.dynamic && route.path === "/plugin/{{PLUGIN_XID}}/pretty") {
    if (!guardText.includes("bSlugPack") || !guardText.includes("Managed_AbilityPackConfigBool(\"content.slug\", \"enablePrettySlugRoute\"")) {
      failures.push(`${file}:${route.line}: slug dynamic route must stay gated by content.slug and enablePrettySlugRoute`);
    }
    continue;
  }
  if (route.dynamic && route.path === "/plugin/{{PLUGIN_XID}}/redirect-pretty") {
    if (!guardText.includes("bRedirectPack") || !guardText.includes("Managed_AbilityPackConfigBool(\"content.redirect\", \"enablePrettyRedirectRoute\"")) {
      failures.push(`${file}:${route.line}: redirect dynamic route must stay gated by content.redirect and enablePrettyRedirectRoute`);
    }
    continue;
  }
  if (route.dynamic) {
    failures.push(`${file}:${route.line}: unexpected dynamic route boundary: ${route.path}`);
    continue;
  }
  if (basePaths.has(route.path)) {
    if (guardText.includes("bViewPack")) {
      failures.push(`${file}:${route.line}: core route must not depend on content.view-stat: ${route.path}`);
    }
    continue;
  }
  if (route.path === "/admin/view/plugin/{{PLUGIN_XID}}/dashboard") {
    if (!guardText.includes("bLikePack") || !guardText.includes("bViewPack")) {
      failures.push(`${file}:${route.line}: dashboard route must stay gated by metric packs`);
    }
    continue;
  }
  if (route.path === "/admin/view/plugin/{{PLUGIN_XID}}/tasks") {
    if (!guardText.includes("bTaskPack")) {
      failures.push(`${file}:${route.line}: task dashboard route must stay gated by background task packs`);
    }
    continue;
  }
  let matched = false;
  for (const [pattern, guard] of required) {
    if (pattern.test(route.path)) {
      matched = true;
      if (!guardText.includes(guard)) {
        failures.push(`${file}:${route.line}: ${route.path} missing ${guard} guard`);
      }
      break;
    }
  }
  if (!matched && route.path.includes("/admin/view/plugin/{{PLUGIN_XID}}/categories")) {
    if (!guardText.includes("bCategoryPack")) failures.push(`${file}:${route.line}: categories view missing bCategoryPack guard`);
  }
}
if (failures.length) {
  console.error(failures.join("\n"));
  process.exit(1);
}
console.log("managed route capability boundary scan OK");
'@
	Invoke-NodeScript 'xadmin_managed_route_capability_boundary_scan.js' $script
}

Invoke-Step 'ability route contract coverage scan' {
	$script = @'
const fs = require("fs");
const path = require("path");
const mainTemplate = fs.readFileSync("hosts/xadmin/data/content/templates/managed_main.c.tpl", "utf8");
const packRoot = "hosts/xadmin/capability-pack";
const declared = new Set();
for (const entry of fs.readdirSync(packRoot, { withFileTypes: true })) {
  if (!entry.isDirectory()) continue;
  const contractsPath = path.join(packRoot, entry.name, "contracts.json");
  if (!fs.existsSync(contractsPath)) continue;
  const contracts = JSON.parse(fs.readFileSync(contractsPath, "utf8"));
  for (const key of [...(contracts.adminApis || []), ...(contracts.publicApis || [])]) {
    declared.add(String(key));
  }
}
function routeKey(routePath) {
  if (routePath === "/api/plugin/{{PLUGIN_XID}}/search" || routePath === "/admin/api/plugin/{{PLUGIN_XID}}/search") {
    return "search.query";
  }
  for (const prefix of ["/admin/api/plugin/{{PLUGIN_XID}}/", "/api/plugin/{{PLUGIN_XID}}/"]) {
    if (routePath.startsWith(prefix)) {
      return routePath.slice(prefix.length).replace(/^\/+|\/+$/g, "").replace(/\//g, ".");
    }
  }
  return "";
}
const coreRoutes = new Set(["meta", "list", "detail", "contracts", "drafts", "pack.meta", "pack.list", "form-meta", "get", "save", "delete"]);
const missing = [];
const routeRe = /route\.path = "([^"]+)"/g;
let match;
while ((match = routeRe.exec(mainTemplate)) !== null) {
  const key = routeKey(match[1]);
  if (!key || coreRoutes.has(key)) continue;
  if (!declared.has(key)) missing.push(`${match[1]} -> ${key}`);
}
if (missing.length) {
  console.error("ability route contracts missing:\n" + missing.join("\n"));
  process.exit(1);
}
console.log("ability route contract coverage scan OK");
'@
	Invoke-NodeScript 'xadmin_ability_route_contract_coverage_scan.js' $script
}

Invoke-Step 'managed category schema boundary scan' {
	$script = @'
const fs = require("fs");
const file = "hosts/xadmin/data/content/templates/managed_main.c.tpl";
const source = fs.readFileSync(file, "utf8");
const fnRe = /(?:void|bool|int|int64|str|xvalue)\s+(Managed_\w+)\s*\([^;]*\)\s*\{/g;

function bodyAt(index) {
  const start = source.indexOf("{", index);
  let depth = 0;
  for (let i = start; i < source.length; i++) {
    if (source[i] === "{") depth++;
    else if (source[i] === "}" && --depth === 0) return source.slice(start, i + 1);
  }
  return "";
}

function lineOf(index) {
  return source.slice(0, index).split(/\r?\n/).length;
}

const allowedNames = new Set([
  "Managed_AccessLoadParentCategory",
  "Managed_CategoryRowExistsForBind",
  "Managed_RequestCategoryListAdmin",
  "Managed_RequestCategoryGetAdmin",
  "Managed_CategoryRewriteDescendantPaths"
]);
const failures = [];
let match;
while ((match = fnRe.exec(source))) {
  const name = match[1];
  const body = bodyAt(match.index);
  if (!/content_category/.test(body)) continue;
  if (/Managed_AbilityPackMounted\("content\.category"\)|bCategoryPack|Managed_RequestCategory|Managed_Category/.test(body)) continue;
  if (allowedNames.has(name)) continue;
  failures.push(`${file}:${lineOf(match.index)}:${name}`);
}
if (failures.length) {
  console.error("content_category referenced outside category capability boundary:\n" + failures.join("\n"));
  process.exit(1);
}
console.log("managed category schema boundary scan OK");
'@
	Invoke-NodeScript 'xadmin_managed_category_schema_boundary_scan.js' $script
}

Invoke-Step 'ability pack list status filter wiring' {
	$mainTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_main.c.tpl')
	$abilityTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_ability.html.tpl')
	foreach ($needle in @(
		'Managed_ReadTextQuery(objReq, "status"',
		'Managed_TableColumnExists(pDb, sTable, "status")',
		'SELECT COUNT(*) FROM %s WHERE status=?',
		'SELECT * FROM %s WHERE status=? ORDER BY id DESC LIMIT ? OFFSET ?'
	)) {
		if ($mainTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_main.c.tpl missing ability pack status filter marker: $needle"
		}
	}
	foreach ($needle in @(
		'/comment/moderation-queue',
		'audit,hide,delete'
	)) {
		if ($abilityTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_ability.html.tpl missing comment audit filter marker: $needle"
		}
	}
	Write-Output 'ability pack list status filter wiring OK'
}

Invoke-Step 'comment tree public response wiring' {
	$mainTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_main.c.tpl')
	foreach ($needle in @(
		'Managed_BuildCommentTree',
		'Managed_CopyCommentNodeWithChildren',
		'Managed_PublicContentVisibleForAccess(pDb, iContentId, objReq, objSession)',
		'xvoTableSetValue(tblRet, "tree", 4',
		'xvoTableSetValue(tblNode, "children", 8',
		'parentId',
		'parent comment not found',
		'SELECT id FROM comment_item WHERE id=? AND content_id=? AND status=1 AND delete_time=0 LIMIT 1',
		'Managed_CommentAntiSpamPass',
		'Managed_CommentDuplicateRecent',
		'Managed_CommentIpRateLimit',
		'Managed_CommentReplyDepthAllowed',
		'Managed_AbilityPackConfigInt("content.comment", "maxReplyDepth", 0)',
		'comment reply depth exceeds',
		'Managed_AbilityPackConfigInt("content.comment", "maxPublicRows", 500)',
		'maxPublicRows',
		'commentStartedAt',
		'maxBodyLength',
		'maxRequestBytes',
		'Managed_CommentMaxRequestBytes',
		'Managed_CommentMaxAdminListRows',
		'maxAdminListRows',
		'comment request body is too large',
		'Managed_RequestCommentStatusBatchAdmin',
		'/comment/status-batch',
		'comment batch exceeds configured limit',
		'Managed_RequestCommentModerationStatsAdmin',
		'/comment/moderation-stats',
		'Managed_RequestCommentModerationQueueAdmin',
		'/comment/moderation-queue',
		'Managed_AppendCommentModerationQueueRow',
		'contentTitle',
		'bodyLength',
		'linkCount',
		'ageSeconds',
		'auditCount',
		'authorNameFilter',
		'bodyFilter',
		'ipFilter',
		"AND (?='' OR c.body LIKE '%' || ? || '%') AND (?='' OR c.ip LIKE '%' || ? || '%')",
		'moderationStatsRecentDays',
		'pendingCount',
		'recentPendingCount',
		'comment status must be pending(0), approve(1) or reject(2)',
		'approve.batch',
		'comment.status.batch',
		'maxAuthorNameLength',
		'Managed_CommentAuthorNameAllowed',
		'Managed_CommentConfigListMatches',
		'allowedAuthorNames',
		'blockedAuthorNames',
		'comment author maxLength is',
		'comment author contains control char',
		'comment author is not in allow list',
		'comment author is blocked',
		'maxUserAgentLength',
		'blockedUserAgentPhrases',
		'Managed_CommentUserAgentAllowed',
		'xsReqHeader(objReq, "User-Agent")',
		'comment userAgent maxLength is',
		'comment userAgent contains control char',
		'comment userAgent is blocked',
		'sqlite3_bind_text(stmt, 7, sUserAgent ? sUserAgent : ""',
		'minBodyLength',
		'minSubmitSeconds',
		'comment body minLength is',
		'blockedBodyPhrases',
		'comment body blocked by phrase list',
		'maxLinks',
		'comment link count exceeds',
		'Managed_CommentModerationRiskFlagsDup',
		'xvoTableSetInt(tblItem, "riskScore", 9',
		'xvoTableSetText(tblItem, "riskFlags", 9',
		'blocked_body_phrase',
		'blocked_user_agent',
		'duplicateWindowSeconds',
		'ipWindowSeconds',
		'ipWindowLimit',
		'comment rate limit exceeded',
		'comment submitted too quickly',
		'comment thread save failed',
		'comment save failed',
		'bInserted = iCommentId > 0 ? TRUE : FALSE',
		'bUpdated = sqlite3_changes(pDb) > 0 ? TRUE : FALSE',
		'bool Managed_RefreshCommentThreadCounts',
		'bool Managed_CommentAuditLog',
		'bool Managed_CommentNotify',
		'xvoTableSetBool(tblRet, "counterRefreshed", 16',
		'xvoTableSetBool(tblRet, "commentAuditSaved", 17',
		'xvoTableSetBool(tblRet, "notificationSaved", 17',
		'ORDER BY create_time ASC,id ASC LIMIT ?',
		'Managed_CommentAuditLog',
		'Managed_RequestCommentAuditLogListAdmin',
		'/comment/audit-log/list',
		"comment_audit_log WHERE (?<=0 OR comment_id=?) AND (?='' OR action=?) ORDER BY create_time DESC,id DESC LIMIT ?",
		'xvoTableSetText(tblRet, "actionFilter", 12, (str)sAction, 0, FALSE)',
		'INSERT INTO comment_audit_log',
		'Managed_CommentNotify',
		'Managed_RequestCommentNotificationListAdmin',
		'/comment/notification/list',
		"comment_notification WHERE (?=999 OR status=?) AND (?<=0 OR comment_id=?) AND (?<=0 OR content_id=?) AND (?='' OR event=?) ORDER BY create_time DESC,id DESC LIMIT ?",
		'xvoTableSetText(tblRet, "eventFilter", 11, (str)sEvent, 0, FALSE)',
		'INSERT INTO comment_notification',
		'comment.created.pending',
		'comment.approved',
		'Managed_SensitiveBeforeComment',
		'Managed_SensitiveScanData(pDb, tblData, "comment"',
		'bSensitiveMarkReview'
	)) {
		if ($mainTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_main.c.tpl missing comment tree marker: $needle"
		}
	}
	foreach ($needle in @('Managed_AppendCommentRiskStat', 'riskScanLimit', 'riskScanned', 'riskStats', 'Managed_CommentModerationRiskFlagsDup(sBody, sUserAgent, iBodyLength, iLinkCount, &iRiskScore)', 'SELECT body,user_agent FROM comment_item WHERE status=0 AND delete_time=0 ORDER BY create_time ASC,id ASC LIMIT ?')) {
		if ($mainTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_main.c.tpl missing comment moderation risk stats marker: $needle"
		}
	}
	$abilityTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_ability.html.tpl')
	foreach ($needle in @('/comment/moderation-queue', 'commentQueueContentId', 'commentQueueAuthorName', 'commentQueueBody', 'commentQueueIp', 'abilityListFilters.commentQueueBody', 'abilityListFilters.commentQueueIp', 'btnCommentQueueFilter', 'btnCommentQueueFilterClear', 'contentTitle', 'bodyLength', 'linkCount', 'riskScore', 'riskFlags', 'auditCount', '/comment/notification/list', 'notifications', '/comment/audit-log/list', 'auditLogs', 'commentAuditCommentId', 'commentAuditAction', 'btnCommentAuditFilter', 'btnCommentAuditFilterClear', 'commentNotificationCommentId', 'commentNotificationContentId', 'commentNotificationStatus', 'commentNotificationEvent', 'btnCommentNotificationFilter', 'btnCommentNotificationFilterClear', 'abilityListFilters.commentNotificationStatus', 'commentId', 'commentBatchIds', 'runCommentStatusBatch', 'renderCommentBatchResult', 'renderCommentBatchRows', 'btnCommentBatchApprove', 'btnCommentBatchReject', 'showCommentModerationStats', 'btnCommentModerationStats', 'commentStatsResult', '/comment/moderation-stats', 'recentPendingCount', 'approvedCount', 'rejectedCount', 'riskScanLimit', 'riskScanned', 'riskStats')) {
		if ($abilityTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_ability.html.tpl missing comment notification marker: $needle"
		}
	}
	$commentContracts = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/capability-pack/content.comment/contracts.json')
	foreach ($needle in @('maxAdminListRows', 'moderationStatsRecentDays', 'adminListLimit', 'adminAuditWorkbench', 'adminModerationQueue', 'adminModerationQueueFilters', 'adminModerationRisk', 'adminModerationRiskStats', 'adminModerationStats', 'adminModerationStatsUi', 'adminBatchModerationUi', 'adminAuditFilters', 'adminBatchModeration', 'adminStatusValidation', 'adminRequestLimit', 'accessIntegration', 'allowedAuthorNames', 'blockedAuthorNames', 'blockedBodyPhrases', 'blockedUserAgentPhrases', 'listControls', 'comment.status-batch', 'comment.moderation-stats', 'comment.moderation-queue')) {
		if ($commentContracts -notmatch [regex]::Escape($needle)) {
			throw "content.comment contracts.json missing admin boundary marker: $needle"
		}
	}
	$commentXform = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/capability-pack/content.comment/instance.xform.json')
	if ($commentXform -notmatch [regex]::Escape('"moderationStatsRecentDays"')) {
		throw 'content.comment instance.xform.json missing moderation stats config marker'
	}
	Write-Output 'comment tree public response wiring OK'
}

Invoke-Step 'sensitive scope and config wiring' {
	$mainTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_main.c.tpl')
	$sensitiveXform = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/capability-pack/content.sensitive/instance.xform.json')
	$sensitivePack = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/capability-pack/content.sensitive/pack.json')
	$sensitiveSchema = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/capability-pack/content.sensitive/schema.sql')
	foreach ($needle in @(
		"scope='' OR scope='all' OR scope=?",
		'sqlite3_bind_text(stmt, 1, sTargetType ? sTargetType : "content"',
		'Managed_AbilityPackConfigTextDup("content.sensitive", "strategy", "block")',
		'Managed_AbilityPackConfigArrayDup("content.sensitive", "fields")',
		'Managed_AbilityPackConfigTextDup("content.sensitive", "matchMode", "substring")',
		'Managed_SensitiveWordValid',
		'content.sensitive", "maxWordLength", 0',
		'content.sensitive", "maxHitsPerScan", 200',
		'if ( iHits >= iMaxHits )',
		'content.sensitive", "maxCheckBytes", 65536',
		'sensitive check request body is too large',
		'sensitive check payload is too large',
		'xvoTableSetInt(tblRet, "maxCheckBytes", 13',
		'content.sensitive", "maxLogListRows", 200',
		'xvoTableSetInt(tblRet, "maxLogListRows", 14',
		'sensitive_hit_log WHERE (?='''' OR target_type=?) AND (?<=0 OR target_id=?) AND (?='''' OR word=?) AND (?='''' OR field_name=?) AND (?='''' OR action=?) ORDER BY create_time DESC,id DESC LIMIT ?',
		'xvoTableSetText(tblRet, "targetTypeFilter", 16',
		'xvoTableSetText(tblRet, "wordFilter", 10',
		'xvoTableSetText(tblRet, "fieldNameFilter", 15',
		'xvoTableSetText(tblRet, "actionFilter", 12',
		'content.sensitive", "maxCleanupRows", 1000',
		'Managed_SensitiveMaxRequestBytes',
		'Managed_SensitiveMaxImportRows',
		'sensitive request body is too large',
		'sensitive import rows exceeded configured limit',
		'sensitive import transaction failed',
		'sensitive import prepare failed',
		'sensitive import commit failed',
		'xvoTableSetText(tblRow, "result", 6, bConfirm ? (bInserted ? "inserted" : "failed") : "preview", 0, FALSE)',
		'sensitive word save failed',
		'sensitive word not found',
		'sensitive cleanup failed',
		'xvoTableSetInt(tblRet, "maxImportRows", 13',
		'DELETE FROM sensitive_hit_log WHERE id IN (SELECT id FROM sensitive_hit_log WHERE create_time<? ORDER BY create_time ASC,id ASC LIMIT ?)',
		'{\"deleted\":%d,\"beforeTime\":%lld,\"limit\":%d}',
		'word length must be <= %d',
		'Managed_TextContainsWordMode',
		'Managed_TextContainsWordLoose',
		'Managed_SensitiveNormalizeLooseText',
		'Managed_SensitiveSkipLooseUtf8Punctuation',
		'zero-width format chars used for obfuscation',
		'p[2] == 0x8B',
		'p[2] == 0xA0',
		'p[1] == 0xBB',
		'strcmp(sMatchMode, "cjkLoose") == 0',
		'Managed_TextReplaceWordMode',
		'xvoTableSetText(tblHit, "matchMode", 9',
		'sNewText = Managed_TextReplaceWordMode(sText, sWord, sSafeReplacement, sMatchMode ? sMatchMode : "substring")',
		'Managed_IsAsciiWordChar',
		'Managed_SensitiveLogRetentionDays',
		'Managed_RequestSensitiveLogCleanupAdmin',
		'Managed_RequestSensitiveWordImportAdmin',
		'Managed_RequestSensitiveStatsAdmin',
		'content.sensitive", "statsRecentDays", 7',
		'/sensitive/stats',
		'xvoTableSetInt(tblData, "wordCount", 9',
		'xvoTableSetInt(tblData, "recentHitCount", 14',
		'xvoTableSetInt(tblData, "hitLogCount", 11',
		'topWords',
		'topGroups',
		'topLimit',
		'SELECT word,COUNT(*),COALESCE(MAX(create_time),0) FROM sensitive_hit_log WHERE create_time>=? GROUP BY word',
		"COALESCE(w.group_key,'unknown')",
		'ALTER TABLE sensitive_word ADD COLUMN group_key',
		'Managed_TableColumnExists(pDb, sTable, "group_key")',
		'xvoTableSetText(tblRet, "groupKey", 8, sGroupKey',
		'UPDATE sensitive_word SET word=?,level=?,scope=?,group_key=?',
		'INSERT INTO sensitive_word(word,level,scope,group_key,replacement,status,create_time,update_time,delete_time)',
		'xvoTableGetText(tblBody, "group_key", 9)',
		'/sensitive/word/import',
		'sensitive.import',
		'content.sensitive", "logRetentionDays", 90',
		'/sensitive/log/cleanup',
		'DELETE FROM sensitive_hit_log WHERE id IN (SELECT id FROM sensitive_hit_log WHERE create_time<? ORDER BY create_time ASC,id ASC LIMIT ?)'
	)) {
		if ($mainTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_main.c.tpl missing sensitive scope marker: $needle"
		}
	}
	foreach ($needle in @('"strategy"', '"fields"', '"body"', '"label"', '"logRetentionDays"', '"statsRecentDays"', '"matchMode"', '"asciiWord"', '"cjkLoose"', '"maxWordLength"', '"maxHitsPerScan"', '"maxCheckBytes"', '"maxLogListRows"', '"maxCleanupRows"', '"maxRequestBytes"', '"maxImportRows"')) {
		if ($sensitiveXform -notmatch [regex]::Escape($needle)) {
			throw "content.sensitive instance.xform.json missing config marker: $needle"
		}
	}
	foreach ($needle in @('"requestLimit"', '"logListLimit"', '"logListFilters"', '"statsSummary"', '"statsTopHits"', '"statsUi"', '"importResultUi"', '"checkResultUi"', '"cleanupResultUi"', '"statsWindow"', '"statsRecentDays"', '"sensitive.stats"', '"hitLimit"', '"maxHitsPerScan"', '"importLimit"', '"groupManagement"', '"boundedImportLimit": "maxImportRows"', '"maxRequestBytes"', '"maxLogListRows"', '"maxImportRows"', '"statusValidation"', '"cjkLooseMatch"', '"cjkLooseZeroWidth"', '"cjkLoose"')) {
		$sensitiveContracts = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/capability-pack/content.sensitive/contracts.json')
		if ($sensitiveContracts -notmatch [regex]::Escape($needle)) {
			throw "content.sensitive contracts.json missing boundary marker: $needle"
		}
	}
	foreach ($needle in @('group_key TEXT NOT NULL DEFAULT', 'idx_sensitive_word_group')) {
		if ($sensitiveSchema -notmatch [regex]::Escape($needle)) {
			throw "content.sensitive schema.sql missing group marker: $needle"
		}
	}
	$abilityTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_ability.html.tpl')
	foreach ($needle in @('runSensitiveImport', 'renderSensitiveImportResult', 'renderSensitiveCheckResult', 'renderSensitiveCleanupResult', 'sensitiveImportJson', 'groupKey', 'sensitiveGroupFilter', 'btnSensitiveGroupFilter', 'btnSensitiveGroupFilterClear', 'btnSensitiveImportPreview', 'btnSensitiveImportConfirm', 'runSensitiveCheck', 'btnSensitiveCheck', 'showSensitiveStats', 'renderSensitiveStats', 'btnSensitiveStats', 'sensitiveStatsResult', 'wordCount', 'recentHitCount', 'hitLogCount', 'topWords', 'topGroups', 'topLimit', '近期高频命中词', '近期高频分组', '/sensitive/stats', 'runSensitiveCleanup', 'btnSensitiveCleanup', 'sensitiveCleanupLimit', 'sensitiveLogTargetType', 'sensitiveLogTargetId', 'sensitiveLogWord', 'sensitiveLogFieldName', 'sensitiveLogAction', 'btnSensitiveLogFilter', 'btnSensitiveLogFilterClear', 'abilityListFilters.sensitiveLogTargetType')) {
		if ($abilityTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_ability.html.tpl missing sensitive import marker: $needle"
		}
	}
	if ($sensitivePack -notmatch [regex]::Escape('/pack/list?pack=content.sensitive')) {
		throw 'content.sensitive pack.json acceptanceApiPath must use existing pack/list route'
	}
	Write-Output 'sensitive scope and config wiring OK'
}

Invoke-Step 'revision visual diff wiring' {
	$abilityTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_ability.html.tpl')
	$editorTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_editor.html.tpl')
	$mainTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_main.c.tpl')
	$revisionContracts = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/capability-pack/content.revision/contracts.json')
	foreach ($needle in @(
		'Managed_RevisionMaxPerContent',
		'bool Managed_RevisionPrune',
		'maxSnapshotsPerContent',
		'content.revision", "maxListRows"',
		'Managed_RevisionMaxRequestBytes',
		'Managed_RevisionValueKind',
		'Managed_RevisionDiffMode',
		'Managed_RevisionNumericValue',
		'Managed_RevisionFieldTypeIsDateTime',
		'Managed_RevisionFieldTypeIsEnum',
		'Managed_RevisionFieldValueLabel',
		'Managed_RevisionCategoryValueLabel',
		'Managed_RevisionContentValueLabel',
		'Managed_RevisionFieldRelationKind',
		'Managed_RevisionResolveRelationLabel',
		'Managed_RevisionSetTypedDiffValues',
		'Managed_RevisionLineCount',
		'xvoTableSetText(tblRow, "fieldType"',
		'xvoTableSetText(tblRow, "diffMode"',
		'xvoTableSetText(tblRow, "changeKind"',
		'xvoTableSetBool(tblRow, "typeChanged"',
		'xvoTableSetInt(tblRow, "beforeLength"',
		'xvoTableSetInt(tblRow, "afterLength"',
		'xvoTableSetInt(tblRow, "beforeLineCount"',
		'xvoTableSetInt(tblRow, "afterLineCount"',
		'xvoTableSetFloat(tblRow, "numberDelta"',
		'xvoTableSetBool(tblRow, "optionLabelResolved"',
		'xvoTableSetText(tblRow, "beforeLabel"',
		'xvoTableSetText(tblRow, "afterLabel"',
		'SELECT title,path FROM content_category WHERE id=?',
		'SELECT title FROM content_item WHERE id=? AND delete_time=0 LIMIT 1',
		'xvoTableSetText(tblRow, "relationKind"',
		'return "relation"',
		'xvoTableSetBool(tblRow, "beforeBool"',
		'xvoTableSetBool(tblRow, "afterBool"',
		'Managed_SetTimeText(tblRow, "beforeTimeText"',
		'Managed_SetTimeText(tblRow, "afterTimeText"',
		'xvoTableSetValue(tblRow, "beforeValue"',
		'xvoTableSetValue(tblRow, "afterValue"',
		'content.revision", "maxRequestBytes", 65536',
		'revision request body is too large',
		'content_revision WHERE (?<=0 OR content_id=?) AND (?='''' OR action=?) AND (?=2147483647 OR status=?) ORDER BY revision_no DESC,id DESC LIMIT ?',
		'Managed_ReadTextQuery(objReq, "action"',
		'xvoTableSetText(tblRet, "actionFilter", 12, sAction',
		'DELETE FROM content_revision WHERE content_id=? AND id NOT IN',
		'Managed_RequestRevisionStatsAdmin',
		'Managed_AppendRevisionActionStat',
		'Managed_AppendRevisionStatusStat',
		'/revision/stats',
		'actionStats',
		'statusStats'
	)) {
		if ($mainTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_main.c.tpl missing revision retention marker: $needle"
		}
	}
	$restoreBody = [regex]::Match($mainTemplate, '(?s)void Managed_RequestRevisionRestoreAdmin\(.*?\n\}\r?\n\r?\n(?:bool|void) Managed_WorkflowNotify').Value
	if ([string]::IsNullOrWhiteSpace($restoreBody)) {
		throw 'managed_main.c.tpl missing revision restore body for derived sync check'
	}
	foreach ($needle in @(
		'bUpdated = sqlite3_changes(pDb) > 0 ? TRUE : FALSE',
		'content not found or deleted',
		'Managed_ContentSyncDerivedData(pDb, tblSpec, iContentId, sTitle, iStatus, iCategoryId > 0 ? iCategoryId : 0, bDraft, tblData, iNow)',
		'Managed_CategoryBindSet(pDb, iContentId, iCategoryId > 0 ? iCategoryId : 0, iNow)',
		'category bind sync failed',
		'content derived sync failed',
		'Managed_StaticMaybeAutoGenerate(pDb, iContentId, bDraft, iStatus)',
		'xvoTableSetInt(tblRet, "revisionId"',
		'xvoTableSetInt(tblRet, "contentId"'
	)) {
		if ($restoreBody -notmatch [regex]::Escape($needle)) {
			throw "managed_main.c.tpl revision restore missing derived sync marker: $needle"
		}
	}
	foreach ($needle in @(
		'renderRevisionChangeTable',
		'renderRevisionValue',
		'renderRevisionMediaValue',
		'renderRevisionStructuredValue',
		'revisionRowValue',
		'row.fieldType',
		'row.diffMode',
		'row.numberDelta',
		'revision-number-diff',
		'row.beforeLabel',
		'row.afterLabel',
		'revision-enum-diff',
		'revision-relation-diff',
		'row.changeKind',
		'row.typeChanged',
		'row.beforeLength',
		'row.afterLength',
		'revision-structured-diff',
		'data-state="',
		'revision-line-diff',
		'revision-media-diff',
		'revision-url-diff',
		'changeCount',
		'renderRevisionChangeTable(changes)',
		'btnRevisionFilter',
		'btnRevisionFilterClear',
		'btnRevisionStats',
		'showRevisionStats',
		'renderRevisionRestoreResult',
		'revisionStatRows',
		'maxRevisionNo',
		'abilityListFilters.revisionContentId',
		'abilityListFilters.revisionAction',
		'abilityListFilters.revisionStatus',
		'revisionFilterAction',
		'revisionFilterStatus',
		"['action', abilityListFilters.revisionAction]",
		'listQuery:function()'
	)) {
		if ($abilityTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_ability.html.tpl missing revision visual diff marker: $needle"
		}
	}
	foreach ($needle in @(
		'renderRevisionChangeDialog',
		'renderRevisionValue',
		'renderRevisionMediaValue',
		'renderRevisionStructuredValue',
		'row.diffMode',
		'row.numberDelta',
		'revision-number-diff',
		'row.beforeLabel',
		'row.afterLabel',
		'revision-enum-diff',
		'revision-relation-diff',
		'revision-structured-diff',
		'data-state="',
		'revision-line-diff',
		'revision-media-diff',
		'revision-url-diff',
		'changeCount',
		'renderRevisionChangeDialog'
	)) {
		if ($editorTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_editor.html.tpl missing revision visual diff marker: $needle"
		}
	}
	foreach ($needle in @(
		'maxListRows',
		'maxRequestBytes',
		'adminContentFilter',
		'adminListFilters',
		'listLimit',
		'requestLimit',
		'revision.stats',
		'adminStats',
		'adminStatsUi',
		'restoreResultUi',
		'categoryBindRestoreSync',
		'typedDiff',
		'numericDiff',
		'booleanDiff',
		'datetimeDiff',
		'enumDiff',
		'relationDiff',
		'customRelationDiff',
		'diffMode',
		'changeKind',
		'typeChanged',
		'beforeLength',
		'afterLength',
		'beforeLineCount',
		'afterLineCount',
		'numberDelta',
		'beforeBool',
		'afterBool',
		'beforeTimeText',
		'afterTimeText',
		'beforeLabel',
		'afterLabel',
		'relationTarget=content'
	)) {
		if ($revisionContracts -notmatch [regex]::Escape($needle)) {
			throw "content.revision contracts.json missing list limit marker: $needle"
		}
	}
	$revisionPack = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/capability-pack/content.revision/pack.json')
	if ($revisionPack -notmatch [regex]::Escape('/revision/list')) {
		throw 'content.revision pack.json acceptanceApiPath must use bounded revision/list route, not diff'
	}
	if ($revisionPack -match [regex]::Escape('/revision/diff')) {
		throw 'content.revision pack.json acceptanceApiPath must not use diff route'
	}
	Write-Output 'revision visual diff wiring OK'
}

Invoke-Step 'media dimension UI wiring' {
	$abilityTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_ability.html.tpl')
	$attachmentUploadPage = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/page/attachment/upload.html')
	$editorTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_editor.html.tpl')
	$mainTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_main.c.tpl')
	$mediaContracts = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/capability-pack/content.media/contracts.json')
	$mediaAttachmentXid = ([string][char]0x9644) + ([string][char]0x4EF6) + 'XID'
	$mediaUploadAttachment = ([string][char]0x4E0A) + ([string][char]0x4F20) + ([string][char]0x9644) + ([string][char]0x4EF6)
	$mediaWidthTitle = "field:'width',title:'" + ([string][char]0x5BBD) + ([string][char]0x5EA6) + "'"
	$mediaHeightTitle = "field:'height',title:'" + ([string][char]0x9AD8) + ([string][char]0x5EA6) + "'"
	foreach ($needle in @(
		'btnMediaDetect_',
		'naturalWidth',
		'naturalHeight',
		$mediaAttachmentXid,
		'/admin/view/attachment/upload',
		'/admin/attachment/get?xid=',
		'btnMediaFill_',
		'fillFromAttachment',
		'mediaRowFromAttachmentData',
		'xadminAttachmentUploaded',
		'window.location.origin',
		'name="attachmentXid"',
		$mediaUploadAttachment,
		$mediaWidthTitle,
		$mediaHeightTitle
	)) {
		if ($abilityTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_ability.html.tpl missing media dimension UI marker: $needle"
		}
	}
	foreach ($needle in @(
		'xadminAttachmentUploaded',
		'window.opener.postMessage',
		'window.location.origin'
	)) {
		if ($attachmentUploadPage -notmatch [regex]::Escape($needle)) {
			throw "attachment upload page missing media postMessage marker: $needle"
		}
	}
	foreach ($needle in @(
		'attachmentFill',
		'attachmentUploadMessage'
	)) {
		if ($mediaContracts -notmatch [regex]::Escape($needle)) {
			throw "content.media contracts.json missing attachment integration marker: $needle"
		}
	}
	foreach ($needle in @(
		'pickCoverMedia',
		'pickBodyMedia',
		'openMediaPicker',
		'/media/list',
		'state.form.setValue(fieldName'
	)) {
		if ($editorTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_editor.html.tpl missing media picker marker: $needle"
		}
	}
	foreach ($needle in @(
		'Managed_MediaValidateMeta',
		'Managed_MediaUrlSafe',
		'Managed_MediaMaybeFillLocalMeta',
		'Managed_MediaReadImageSize',
		'Managed_MediaLocalStaticPath',
		'Managed_MediaMaxAssetSizeBytes',
		'Managed_MediaMaxImageDimension',
		'Managed_MediaMimeAllowed',
		'Managed_MediaPublicAllowed',
		'content.media", "maxPublicRefCheckRows", 100',
		'content_media_ref WHERE media_id=? ORDER BY id ASC LIMIT ?',
		'allowedMimeTypes',
		'media mime is not allowed',
		'content.media", "maxDetailMediaRows"',
		'content_media_ref r INNER JOIN content_media m ON m.id=r.media_id WHERE r.content_id=? AND m.delete_time=0 ORDER BY r.id ASC LIMIT ?',
		'content.media", "maxListRows"',
		'Managed_ReadTextQuery(objReq, "mime"',
		'Managed_ReadTextQuery(objReq, "status"',
		"(?='' OR m.mime=?)",
		'(?=2147483647 OR m.status=?)',
		'ORDER BY m.id DESC LIMIT ?',
		'maxAssetSizeBytes',
		'maxImageDimension',
		'serverDetectImageSize',
		'content.media", "maxRefListRows"',
		'content_media_ref r INNER JOIN content_media m ON m.id=r.media_id',
		'maxRefListRows',
		'Managed_MediaMaxRequestBytes',
		'Managed_MediaMaxBatchRows',
		'media request body is too large',
		'media batch rows exceeded configured limit',
		'unsupported media batch action',
		'{\"action\":\"%s\",\"total\":%d,\"success\":%d,\"failed\":%d}',
		'GIF89a',
		'IHDR',
		'0xff',
		'media url must be relative, http, or https',
		'media dimensions must be non-negative',
		'media dimensions must be <= %d',
		'media save failed',
		'media not found'
	)) {
		if ($mainTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_main.c.tpl missing media metadata validation marker: $needle"
		}
	}
	$mediaContracts = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/capability-pack/content.media/contracts.json')
	foreach ($needle in @('maxBatchRows', 'maxRequestBytes', 'maxPublicRefCheckRows', 'publicRefCheckLimit', 'adminListFilters', 'batchLimit', 'batchResultUi', 'requestLimit', 'accessIntegration')) {
		if ($mediaContracts -notmatch [regex]::Escape($needle)) {
			throw "content.media contracts.json missing media boundary marker: $needle"
		}
	}
	foreach ($needle in @('btnMediaFilter', 'btnMediaFilterClear', 'mediaFilterMime', 'mediaFilterStatus', 'renderGenericBatchResult', 'renderGenericBatchRows', "['mime', abilityListFilters.mediaMime]", "['status', abilityListFilters.mediaStatus]")) {
		if ($abilityTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_ability.html.tpl missing media filter UI marker: $needle"
		}
	}
	Write-Output 'media dimension UI wiring OK'
}

Invoke-Step 'category drag sort wiring' {
	$categoryTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_category.html.tpl')
	$mainTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_main.c.tpl')
	foreach ($needle in @(
		'DragSort_{{PLUGIN_DOM_ID_BASE}}',
		'openDragSort',
		'managed-category-drag-item',
		'/category/sort',
		'dragParentOptions',
		'managed-category-drag-parent',
		'暂无栏目',
		'ExpandAll_{{PLUGIN_DOM_ID_BASE}}',
		'CollapseAll_{{PLUGIN_DOM_ID_BASE}}',
		'visibleCategoryRows',
		'toggleCategoryCollapsed',
		'categoryCollapsedCount',
		'updateCategorySummary',
		'renderCategoryTreeTitle',
		'managed-category-tree-toggle',
		'managed-category-summary',
		'CategorySummary_{{PLUGIN_DOM_ID_BASE}}',
		'parentId: Number(select && select.value || 0)',
		'row.breadcrumb || row.treeTitle',
		'CategoryFilter_{{PLUGIN_DOM_ID_BASE}}',
		'buildCategoryRowMaps',
		'markCategoryFilterContext',
		'filterCategoryRows',
		'rowContainsKeyword',
		'keyword ? filteredRows : visibleCategoryRows(filteredRows)',
		'filtered.length',
		'renderCategorySortRows',
		'showCategorySortResult',
		'栏目排序结果',
		"{ field: 'childCount', width: 100"
	)) {
		if ($categoryTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_category.html.tpl missing category drag sort marker: $needle"
		}
	}
	foreach ($needle in @(
		'xvoTableExists(tblItem, "parentId", 8)',
		'bool Managed_CategoryRewriteDescendantPaths',
		'Managed_CategoryRewriteDescendantPaths(pDb, iId',
		'descendantPathsUpdated',
		'descendantPathFailures',
		'Managed_CategorySlugValid',
		'Managed_CategoryMaxDepth',
		'Managed_CategoryMaxRequestBytes',
		'Managed_CategoryMaxSortRows',
		'maxDepth',
		'content.category", "maxPublicContentRows"',
		'content.category", "maxAdminTreeRows"',
		'content.category", "maxPublicTreeRows"',
		'content.category", "maxRequestBytes", 65536',
		'content.category", "maxSortRows", 500',
		'Managed_AbilityPackMounted("content.category") && !Managed_TableColumnExists(pDb, "content_category", "description")',
		'Managed_AbilityPackMounted("content.category") && !Managed_TableColumnExists(pDb, "content_category", "cover_url")',
		'Managed_AbilityPackMounted("content.category") && !Managed_TableColumnExists(pDb, "content_category", "template_key")',
		'if ( bOK && Managed_AbilityPackMounted("content.category") )',
		'bOK = Managed_ExecSql(pDb, G_CategoryPostMigrationIndexSql)',
		'Managed_CategoryBindSet',
		'Managed_CategoryBindLoad',
		'Managed_CategoryBindCountSql',
		'Managed_AppendCategoryBindDriftSamples',
		'Managed_CategoryBindApplyToItem',
		'Managed_CategoryBindBackfillLegacy',
		'Managed_RequestCategoryBindStatusAdmin',
		'Managed_RequestCategoryBindBackfillAdmin',
		'category bind sync failed',
		'/category/bind/status',
		'/category/bind/backfill',
		'legacyContentRows',
		'missingBindRows',
		'mismatchRows',
		'missingSamples',
		'mismatchSamples',
		'sampleLimit',
		'legacyCategoryId',
		'bindCategoryId',
		'Managed_AbilityPackConfigInt("content.category", "maxDriftSampleRows", 10)',
		'ORDER BY i.id ASC LIMIT ?',
		'adminTimeOnly',
		'INSERT INTO content_category_bind(content_id,category_id,status,create_time,update_time,delete_time)',
		'NOT EXISTS (SELECT 1 FROM content_category_bind cb WHERE cb.content_id=i.id AND cb.delete_time=0)',
		'int64 iBoundCategoryId = Managed_CategoryBindLoad(pDb, iContentId)',
		'content_category_bind',
		'INNER JOIN content_item i ON i.id=cb.content_id',
		'category request body is too large',
		'category sort rows exceeded configured limit',
		'xvalue arrRows = xvoCreateArray()',
		'xvoTableSetInt(tblRow, "rowIndex", 8',
		'xvoTableSetBool(tblRow, "moveParent", 10',
		'xvoTableSetBool(tblRow, "updated", 7',
		'xvoTableSetBool(tblRow, "descendantPathsUpdated", 22',
		'xvoTableSetValue(tblRet, "rows", 4, arrRows, TRUE)',
		'FROM content_category c WHERE c.delete_time = 0 ORDER BY c.parent_id ASC, c.sort ASC, c.id ASC LIMIT ?',
		'FROM content_category c WHERE c.delete_time = 0 AND c.status = 1 ORDER BY c.path ASC, c.sort ASC, c.id ASC LIMIT ?',
		'AS child_count',
		'xvoTableSetInt(tblItem, "childCount", 10',
		'Managed_CategoryMaskPublicCountForAccess',
		'contentCountHiddenByAccess',
		'WHERE cb.category_id=? AND cb.status=1 AND cb.delete_time=0 AND i.delete_time=0 AND i.is_draft=0 AND i.status >= %d ORDER BY i.update_time DESC,i.id DESC LIMIT ?',
		'Managed_AccessCheckRule(pDb, xvoTableGetInt(tblItem, "id", 2), objReq, objSession, FALSE, NULL)',
		'category tree exceeds maxDepth',
		'category slug is invalid',
		'category save failed',
		'category not found',
		'category has child categories',
		'xvoTableSetInt(tblRet, "childCount", 10, iChildCount)',
		'xvoTableSetInt(tblRet, "contentCount", 12, iContentCount)',
		'bDeleted = sqlite3_changes(pDb) > 0 ? TRUE : FALSE',
		'xvoTableSetText(tblItem, "breadcrumb", 10',
		'parent_id=?, path=?, level=?, sort=?, update_time=?',
		'parent category cannot be descendant'
	)) {
		if ($mainTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_main.c.tpl missing category parent move marker: $needle"
		}
	}
	$categoryContracts = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/capability-pack/content.category/contracts.json')
	$categorySchemaPath = Join-Path $Root 'hosts/xadmin/capability-pack/content.category/schema.sql'
	if (!(Test-Path $categorySchemaPath)) {
		throw 'content.category schema.sql missing'
	}
	$categorySchema = Get-Content -Raw -Encoding UTF8 $categorySchemaPath
	foreach ($needle in @('CREATE TABLE IF NOT EXISTS content_category', 'idx_content_category_parent_sort', 'idx_content_category_parent_slug', 'CREATE TABLE IF NOT EXISTS content_category_bind', 'idx_content_category_bind_content', 'idx_content_category_bind_category')) {
		if ($categorySchema -notmatch [regex]::Escape($needle)) {
			throw "content.category schema.sql missing category schema marker: $needle"
		}
	}
	$abilityTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_ability.html.tpl')
	foreach ($needle in @('content.category', 'btnCategoryBindStatus', 'btnCategoryBindBackfill', 'showCategoryBindStatus', 'runCategoryBindBackfill', 'renderCategoryBindStatus', 'renderCategoryBindSampleRows', 'renderCategoryBindBackfillResult', 'legacyContentRows', 'activeBindRows', 'missingBindRows', 'mismatchRows', 'missingSamples', 'mismatchSamples', 'sampleLimit', '/category/bind/status', '/category/bind/backfill', 'categoryBindStatusResult')) {
		if ($abilityTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_ability.html.tpl missing category bind migration marker: $needle"
		}
	}
	foreach ($needle in @('content_category_bind', 'categoryBindTable', 'categoryBindBackfill', 'category.bind.status', 'category.bind.backfill', 'categoryBindDiagnostics', 'categoryBindDriftSamples', 'categoryBindAdminBackfill', 'categoryBindDiagnosticsUi', 'categoryBindBackfillUi', 'categoryBindWritePaths', 'sortResultRows', 'sortResultUi', 'maxSortRows', 'maxRequestBytes', 'maxDriftSampleRows', 'sortLimit', 'requestLimit', 'accessIntegration', 'adminTreeFilter', 'treeObservability', 'treeUi', 'treeSummaryUi', 'treeFilterContextUi', 'deleteProtectionCounts', 'statusValidation')) {
		if ($categoryContracts -notmatch [regex]::Escape($needle)) {
			throw "content.category contracts.json missing category boundary marker: $needle"
		}
	}
	Write-Output 'category drag sort wiring OK'
}

Invoke-Step 'seo meta validation wiring' {
	$mainTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_main.c.tpl')
	$publicTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_public.html.tpl')
	$abilityTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_ability.html.tpl')
	$capabilityEditor = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/wwwroot/content/js/editor-capability.js')
	$seoContracts = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/capability-pack/content.seo/contracts.json')
	foreach ($needle in @(
		'Managed_SeoMetaValid',
		'Managed_SeoCanonicalSafe',
		'Managed_SeoApplyTemplate',
		'Managed_BuildSeoTemplateVariables',
		'templateVariables',
		'Managed_SeoConfigText',
		'Managed_AbilityPackConfig(tblContracts, "content.seo")',
		'content.seo", "maxListRows"',
		'Managed_SeoMaxRequestBytes',
		'content.seo", "maxRequestBytes", 65536',
		'seo request body is too large',
		'content_seo_meta m LEFT JOIN content_item c ON c.id=m.content_id WHERE m.delete_time=0 ORDER BY m.update_time DESC,m.id DESC LIMIT ?',
		'Managed_AccessCheckRule(pDb, xvoTableGetInt(tblData, "id", 2), objReq, objSession, FALSE, NULL)',
		'seoTitle is too long',
		'canonical must be relative, http, or https',
		'seo meta save failed',
		'seo content not found',
		'seo meta not found'
	)) {
		if ($mainTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_main.c.tpl missing seo meta validation marker: $needle"
		}
	}
	foreach ($needle in @(
		'mpAbilityPackConfig',
		'mpApplyTextTemplate',
		'categoryTitleTemplate',
		'categoryKeywordsTemplate',
		'categoryDescriptionTemplate',
		'categoryCanonicalTemplate',
		'siteName: pluginTitle'
	)) {
		if ($publicTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_public.html.tpl missing seo category template marker: $needle"
		}
	}
	foreach ($needle in @(
		"key === 'content.seo'",
		'categoryTitleTemplate',
		'categoryCanonicalTemplate',
		'{categoryPath}'
	)) {
		if ($capabilityEditor -notmatch [regex]::Escape($needle)) {
			throw "editor-capability.js missing seo config marker: $needle"
		}
	}
	foreach ($needle in @(
		'runSeoPreview',
		'seoPreviewContentId',
		'seoPreviewSlug',
		'seoPreviewResult',
		'btnSeoPreview',
		'runSeoCategoryPreview',
		'seoPreviewCategoryId',
		'seoPreviewCategorySlug',
		'seoCategoryPreviewResult',
		'btnSeoCategoryPreview',
		'/category/detail',
		'abilityApplyTextTemplate',
		'renderSeoPreviewResult',
		'renderSeoVariableRows',
		'collectTemplateTokens',
		'attachSeoTemplateWarnings',
		'meta.warnings',
		'{contentId}',
		'{pluginTitle}',
		'{seoDescription}',
		'{categoryTitle}'
	)) {
		if ($abilityTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_ability.html.tpl missing seo preview marker: $needle"
		}
	}
	foreach ($needle in @(
		'maxListRows',
		'maxRequestBytes',
		'listLimit',
		'requestLimit',
		'previewUi',
		'previewResultUi',
		'variableValuePreview',
		'categoryVariablePreview',
		'templateWarningPreview',
		'accessIntegration',
		'contentTemplateVariables',
		'categoryTemplateVariables',
		'statusValidation'
	)) {
		if ($seoContracts -notmatch [regex]::Escape($needle)) {
			throw "content.seo contracts.json missing list limit marker: $needle"
		}
	}
	Write-Output 'seo meta validation wiring OK'
}

Invoke-Step 'capability admin status normalization wiring' {
	$mainTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_main.c.tpl')
	foreach ($name in @(
		'Managed_RequestSensitiveWordSaveAdmin',
		'Managed_RequestRelatedSaveAdmin',
		'Managed_RequestSeoMetaSaveAdmin',
		'Managed_RequestCategorySaveAdmin'
	)) {
		$body = [regex]::Match($mainTemplate, "(?s)void $name\(.*?(?=\r?\nvoid |\r?\nstatic |\z)")
		if ([string]::IsNullOrWhiteSpace($body.Value)) {
			throw "managed_main.c.tpl missing status-normalized function: $name"
		}
		if ($body.Value -notmatch [regex]::Escape('iStatus = iStatus ? 1 : 0;')) {
			throw "managed_main.c.tpl missing status normalization in $name"
		}
		if ($body.Value -match [regex]::Escape('if ( iStatus <= 0 ) iStatus = 1;')) {
			throw "managed_main.c.tpl keeps legacy status defaulting in $name"
		}
	}
	$importBody = [regex]::Match($mainTemplate, '(?s)void Managed_RequestSensitiveWordImportAdmin\(.*?(?=\r?\nvoid |\r?\nstatic |\z)')
	if ([string]::IsNullOrWhiteSpace($importBody.Value) -or ($importBody.Value -notmatch [regex]::Escape('iStatus = iStatus ? 1 : 0;'))) {
		throw 'managed_main.c.tpl missing sensitive import status normalization'
	}
	Write-Output 'capability admin status normalization wiring OK'
}

Invoke-Step 'form schema designer wiring' {
	$abilityTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_ability.html.tpl')
	$mainTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_main.c.tpl')
	$formContracts = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/capability-pack/content.form/contracts.json')
	foreach ($needle in @(
		'appendFormSchemaField',
		'updateFormSchemaField',
		'removeFormSchemaField',
		'moveFormSchemaField',
		'btnFormDesignerAdd',
		'btnFormDesignerUpdate',
		'btnFormDesignerRemove',
		'btnFormDesignerUp',
		'btnFormDesignerDown',
		'btnFormSchemaFormat',
		'btnFormSchemaValidate',
		'btnFormSubmissionFilter',
		'btnFormSubmissionFilterClear',
		'btnFormSubmissionStats',
		'showFormSubmissionStats',
		'renderFormSubmissionStats',
		'renderFormSubmissionExportResult',
		'renderFormSubmissionExportRows',
		'renderFormSubmissionStatusRows',
		'renderFormSubmissionFormRows',
		'formSubmissionFormId',
		'formSubmissionContentId',
		'formSubmissionStatus',
		'abilityListFilters.formSubmissionFormId',
		'btnFormNotificationFilter',
		'btnFormNotificationFilterClear',
		'btnFormNotificationStats',
		'showFormNotificationStats',
		'eventStats',
		'localNotificationOnly',
		'lastCreateTimeText',
		'formNotificationFormId',
		'formNotificationContentId',
		'formNotificationStatus',
		'formNotificationEvent',
		'abilityListFilters.formNotificationFormId',
		'formDesignerPlaceholder',
		'formDesignerPattern',
		'formDesignerOptions',
		'minLength',
		'maxLength',
		'type:''textarea'',value:''{}'''
	)) {
		if ($abilityTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_ability.html.tpl missing form schema designer marker: $needle"
		}
	}
	foreach ($needle in @(
		'Managed_FormValueMatchesOptions',
		'Managed_FormOptionContainsValue',
		'Managed_FormSubmissionMaxBytes',
		'Managed_FormMaxRequestBytes',
		'Managed_AbilityPackConfigInt("content.form", "maxRequestBytes", 262144)',
		'form request body is too large',
		'Managed_PublicContentVisibleForAccess(pDb, iContentId, objReq, objSession)',
		'Managed_AbilityPackConfigInt("content.form", "maxSubmissionBytes", 262144)',
		'form submission is too large',
		'Managed_FormValidateDefinition',
		'Managed_AbilityPackConfigInt("content.form", "maxFields", 100)',
		'form schema fields count exceeds',
		'form schema field #%d must be an object',
		'form schema field #%d name is required',
		'form schema field name duplicated',
		'form schema required must be an array',
		'form schema required #%d must be a field name',
		'form schema required field not found',
		'form schema required field duplicated',
		'Managed_FormIpRateLimit',
		'Managed_AbilityPackConfigInt("content.form", "minSubmitSeconds", 2)',
		'formStartedAt',
		'form submission rate limit exceeded',
		'Managed_ResolveFieldList(tblField)',
		'minItems',
		'maxItems',
		'strcmp(sFormat, "phone")',
		'option is invalid',
		'Managed_FormNotify',
		'Managed_BackgroundTaskCreate(pDb, "content.form.notification.deliver"',
		'Managed_BackgroundTaskCreate(pDb, "content.form.notification.replay"',
		'Managed_RequestFormSubmissionStatsAdmin',
		'Managed_AppendFormSubmissionStatusStat',
		'Managed_AppendFormSubmissionFormStat',
		'Managed_RequestFormNotificationListAdmin',
		'Managed_RequestFormNotificationStatusAdmin',
		'Managed_RequestFormNotificationStatsAdmin',
		'Managed_RequestFormNotificationReplayAdmin',
		'content_form_notification',
		'localNotificationOnly',
		'deliveryPolicy',
		'SELECT event,COUNT(*),SUM(CASE WHEN status=0 THEN 1 ELSE 0 END),MAX(create_time) FROM content_form_notification GROUP BY event ORDER BY COUNT(*) DESC,event ASC LIMIT ?',
		'maxNotificationStatEvents',
		'Managed_ReadTextQuery(objReq, "formId"',
		'Managed_ReadTextQuery(objReq, "contentId"',
		'Managed_ReadTextQuery(objReq, "event"',
		'(?<=0 OR form_id=?) AND (?<=0 OR content_id=?) AND (?=2147483647 OR status=?) AND (?='''' OR event=?)',
		'/form/notification/list',
		'/form/notification/status',
		'/form/notification/stats',
		'/form/notification/replay',
		'form.notification.status',
		'form.notification.replay',
		'backgroundTaskId',
		'queued',
		'sourceNotificationId',
		'newNotificationId',
		'read_time',
		'readTimeText',
		'process_note',
		'processed_by',
		'process_time',
		'processNote',
		'processedBy',
		'processTime',
		'content.form", "maxListRows"',
		'content.form", "maxExportRows"',
		'(?<=0 OR s.form_id=?)',
		'(?<=0 OR s.content_id=?)',
		'(?=999 OR s.status=?)',
		'/form/submission/stats',
		'statusStats',
		'formStats',
		'formStatLimit',
		'xvoTableSetInt(tblRet, "statusFilter", 12, iStatusFilter)',
		'LIMIT ?',
		'form save failed',
		'form content not found',
		'form not found',
		'form submission not found',
		'form submission status must be 0, 1 or 2',
		'form notification status must be 0 or 1',
		'form submission save failed',
		'bSaved = sqlite3_changes(pDb) > 0 ? TRUE : FALSE',
		'bDeleted = sqlite3_changes(pDb) > 0 ? TRUE : FALSE',
		'bUpdated = sqlite3_changes(pDb) > 0 ? TRUE : FALSE',
		'bInserted = iSubmissionId > 0 ? TRUE : FALSE',
		'xvoTableSetInt(tblRet, "submissionId", 12, iSubmissionId)'
	)) {
		if ($mainTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_main.c.tpl missing form option validation marker: $needle"
		}
	}
	foreach ($needle in @(
		'maxListRows',
		'maxExportRows',
		'maxFields',
		'maxRequestBytes',
		'schemaDesigner',
		'schemaFieldNameValidation',
		'updateField',
		'submissionStats',
		'form.submission.stats',
		'submissionStatsUi',
		'submissionExportUi',
		'notificationStatus',
		'notificationStats',
		'notificationStatsUi',
		'notificationDeliveryTask',
		'notificationReplay',
		'notificationReplayTaskUi',
		'designerPreviewUi',
		'accessIntegration',
		'requestLimit',
		'statusValidation',
		'listLimit',
		'submissionFilters',
		'notificationFilters',
		'exportLimit'
	)) {
		if ($formContracts -notmatch [regex]::Escape($needle)) {
			throw "content.form contracts.json missing list/export limit marker: $needle"
		}
	}
	foreach ($needle in @('/form/submission/stats', 'btnFormSubmissionStats', 'showFormSubmissionStats', 'renderFormSubmissionStats', 'renderFormSubmissionExportResult', '/form/notification/list', '/form/notification/status', '/form/notification/stats', '/form/notification/replay', 'notifications', 'submissionId', 'markFormNotificationStatus', 'replayFormNotification', 'markRead', 'markUnread', 'replay', 'readTimeText', "['event', abilityListFilters.formNotificationEvent]")) {
		if ($abilityTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_ability.html.tpl missing form notification marker: $needle"
		}
	}
	foreach ($needle in @('formDesignerPreview', 'btnFormDesignerPreview', 'renderFormSchemaDesignerPreview', 'fillFormDesignerField', 'data-form-designer-pick', 'analyzeFormSchema', 'issueMap', 'issue.message', 'fieldNames', 'requiredSeen', 'required.forEach')) {
		if ($abilityTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_ability.html.tpl missing form designer preview marker: $needle"
		}
	}
	$formPack = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/capability-pack/content.form/pack.json')
	if ($formPack -notmatch [regex]::Escape('/form/submission/list')) {
		throw 'content.form pack.json acceptanceApiPath must use bounded submission/list route, not export'
	}
	if ($formPack -match [regex]::Escape('/form/submission/export')) {
		throw 'content.form pack.json acceptanceApiPath must not use export route'
	}
	Write-Output 'form schema designer wiring OK'
}

Invoke-Step 'tag topic config wiring' {
	$mainTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_main.c.tpl')
	$abilityTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_ability.html.tpl')
	$tagContracts = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/capability-pack/content.tag/contracts.json')
	$tagXform = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/capability-pack/content.tag/instance.xform.json')
	$topicXform = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/capability-pack/content.topic/instance.xform.json')
	foreach ($needle in @(
		'Managed_AbilityPackConfigInt("content.tag", "maxTags", 0)',
		'Managed_AbilityPackConfigBool("content.tag", "allowCreateInline", TRUE)',
		'content.tag", "maxPublicListRows"',
		'content.tag", "maxPublicContentRows"',
		'content.tag", "maxAdminLinkRows"',
		'Managed_TagMaxRequestBytes',
		'Managed_TagMaxBatchRows',
		'content.tag", "maxBatchRows"',
		'tag request body is too large',
		'Managed_RequestTagBatchStatusAdmin',
		'/tag/batch-status',
		'Managed_RequestTagStatsAdmin',
		'Managed_AppendTagTopStat',
		'Managed_AppendTagStatusStat',
		'/tag/stats',
		'statusStats',
		'topTagStats',
		'linkCount',
		'topStatLimit',
		'tag batch rows exceeded configured limit',
		'tag.batch-status',
		'Managed_MaskPublicCountForAccess',
		'contentCountHiddenByAccess',
		'FROM tag WHERE status=1 AND delete_time=0 ORDER BY sort ASC,id ASC LIMIT ?',
		'content_tag ct LEFT JOIN tag t ON t.id=ct.tag_id LEFT JOIN content_item c ON c.id=ct.content_id ORDER BY ct.create_time DESC,ct.id DESC LIMIT ?',
		'WHERE ct.content_id=? ORDER BY ct.create_time DESC,ct.id DESC LIMIT ?',
		'WHERE ct.tag_id=? ORDER BY ct.create_time DESC,ct.id DESC LIMIT ?',
		'tag save failed',
		'tag not found',
		'tag relation not found',
		'tag delete transaction failed',
		'tag relation cleanup failed',
		'tag delete commit failed',
		'bool Managed_UpdateTagCounters',
		'tag counter save failed',
		'Managed_RequestTagMergeAdmin',
		'/tag/merge',
		'source tag not found',
		'tag merge transaction failed',
		'tag merge commit failed',
		'sourceDeleted',
		'INSERT OR IGNORE INTO content_tag(content_id,tag_id,sort,create_time) SELECT content_id,?,sort,create_time FROM content_tag WHERE tag_id=?',
		'tag.merge',
		'Managed_TagRowExists',
		'SELECT 1 FROM tag WHERE id=? AND delete_time=0 LIMIT 1',
		'tag bind transaction failed',
		'tag bind target not found',
		'tag bind commit failed',
		'Managed_AbilityPackConfigTextDup("content.topic", "mode", "single")',
		'Managed_AbilityPackConfigInt("content.topic", "maxBindContents", 200)',
		'content.topic", "maxPublicListRows"',
		'content.topic", "maxPublicContentRows"',
		'content.topic", "maxAdminLinkRows"',
		'Managed_TopicMaxRequestBytes',
		'Managed_TopicMaxBatchRows',
		'content.topic", "maxBatchRows"',
		'topic request body is too large',
		'Managed_RequestTopicBatchStatusAdmin',
		'/topic/batch-status',
		'Managed_RequestTopicStatsAdmin',
		'Managed_AppendTopicTopStat',
		'Managed_AppendTopicStatusStat',
		'/topic/stats',
		'statusStats',
		'topTopicStats',
		'linkCount',
		'topStatLimit',
		'topic batch rows exceeded configured limit',
		'topic.batch-status',
		'FROM topic WHERE status=1 AND delete_time=0 ORDER BY sort ASC,id ASC LIMIT ?',
		'topic_content tc LEFT JOIN topic t ON t.id=tc.topic_id LEFT JOIN content_item c ON c.id=tc.content_id ORDER BY tc.sort ASC,tc.create_time DESC,tc.id DESC LIMIT ?',
		'WHERE tc.content_id=? ORDER BY tc.sort ASC,tc.create_time DESC,tc.id DESC LIMIT ?',
		'WHERE tc.topic_id=? ORDER BY tc.sort ASC,tc.create_time DESC,tc.id DESC LIMIT ?',
		'topic save failed',
		'topic not found',
		'topic relation not found',
		'topic delete transaction failed',
		'topic relation cleanup failed',
		'topic delete commit failed',
		'bool Managed_UpdateTopicCounters',
		'topic counter save failed',
		'bDeleted = sqlite3_changes(pDb) > 0 ? TRUE : FALSE',
		'Managed_RequestTopicContentSortAdmin',
		'/topic/content/sort',
		'items count must be <= 500',
		'topic content sort target not found',
		'topic content sort transaction failed',
		'topic content sort commit failed',
		'topic.content.sort',
		'contentIds count exceeds maxBindContents',
		'topicIds count exceeds maxBindContents',
		'topic bind transaction failed',
		'topic bind target not found',
		'topic bind commit failed',
		'Managed_TopicRowExists',
		'Managed_ContentRowExistsForBind',
		'Managed_CategoryBindApplyToItem(pDb, tblItem)',
		'SELECT 1 FROM topic WHERE id=? AND delete_time=0 LIMIT 1',
		'SELECT 1 FROM content_item WHERE id=? AND delete_time=0 LIMIT 1',
		'xvoTableSetInt(tblRet, "insertedCount", 13, iInserted)',
		'Managed_AccessCheckRule(pDb, xvoTableGetInt(tblItem, "id", 2), objReq, objSession, FALSE, NULL)'
	)) {
		if ($mainTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_main.c.tpl missing tag/topic config marker: $needle"
		}
	}
	foreach ($needle in @('runTagMerge', 'renderTagMergeResult', 'tagMergeSourceId', 'tagMergeTargetId', 'btnTagMerge')) {
		if ($abilityTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_ability.html.tpl missing tag merge UI marker: $needle"
		}
	}
	foreach ($needle in @('tagBatchIds', 'runTagBatchStatus', 'renderGenericBatchResult', 'btnTagBatchEnable', 'btnTagBatchDisable', '/tag/batch-status')) {
		if ($abilityTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_ability.html.tpl missing tag batch status UI marker: $needle"
		}
	}
	foreach ($needle in @('btnTagStats', 'showTagStats', '/tag/stats', 'renderTaxonomyStats', 'topTagStats', 'relationCount', 'topStatLimit')) {
		if ($abilityTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_ability.html.tpl missing tag stats UI marker: $needle"
		}
	}
	foreach ($needle in @('btnTagLinkFilter', 'btnTagLinkFilterClear', 'tagLinkTagId', 'tagLinkContentId', "['tagId', abilityListFilters.tagLinkTagId]", "['contentId', abilityListFilters.tagLinkContentId]")) {
		if ($abilityTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_ability.html.tpl missing tag relation filter marker: $needle"
		}
	}
	foreach ($needle in @('runTopicSort', 'renderTopicSortResult', 'topicSortItems', 'topicSortResult', 'btnTopicSort', 'loadTopicArrange', 'topicArrangeRows', 'topicArrangeList', 'data-topic-move', 'btnTopicArrangeLoad')) {
		if ($abilityTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_ability.html.tpl missing topic sort UI marker: $needle"
		}
	}
	foreach ($needle in @('topicBatchIds', 'runTopicBatchStatus', 'renderGenericBatchResult', 'btnTopicBatchEnable', 'btnTopicBatchDisable', '/topic/batch-status')) {
		if ($abilityTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_ability.html.tpl missing topic batch status UI marker: $needle"
		}
	}
	foreach ($needle in @('btnTopicStats', 'showTopicStats', '/topic/stats', 'renderTaxonomyStats', 'topTopicStats', 'relationCount', 'topStatLimit')) {
		if ($abilityTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_ability.html.tpl missing topic stats UI marker: $needle"
		}
	}
	foreach ($needle in @('btnTopicLinkFilter', 'btnTopicLinkFilterClear', 'topicLinkTopicId', 'topicLinkContentId', "['topicId', abilityListFilters.topicLinkTopicId]", "['contentId', abilityListFilters.topicLinkContentId]")) {
		if ($abilityTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_ability.html.tpl missing topic relation filter marker: $needle"
		}
	}
	foreach ($needle in @('"tag.merge"', '"tag.afterMerge"', '"mergeBehavior"')) {
		if ($tagContracts -notmatch [regex]::Escape($needle)) {
			throw "content.tag contracts.json missing tag merge marker: $needle"
		}
	}
	foreach ($needle in @('"publicListLimit"', '"detailTagLimit"', '"publicContentLimit"', '"adminLinkLimit"', '"adminLinkFilters"', '"adminStats"', '"adminStatsUi"', '"mergeResultUi"', '"maxRequestBytes"', '"maxBatchRows"', '"requestLimit"', '"batchStatus"', '"batchStatusUi"', '"batchLimit"', '"statusValidation"', '"categoryBindIntegration"', '"accessIntegration"', '"tag.batch-status"', '"tag.stats"')) {
		if ($tagContracts -notmatch [regex]::Escape($needle)) {
			throw "content.tag contracts.json missing list limit marker: $needle"
		}
	}
	foreach ($needle in @('"maxTags"', '"allowCreateInline"', '"maxPublicListRows"', '"maxDetailTags"', '"maxPublicContentRows"', '"maxAdminLinkRows"', '"maxRequestBytes"', '"maxBatchRows"', '"label"')) {
		if ($tagXform -notmatch [regex]::Escape($needle)) {
			throw "content.tag instance.xform.json missing marker: $needle"
		}
	}
	foreach ($needle in @('"mode"', '"maxBindContents"', '"maxPublicListRows"', '"maxDetailTopics"', '"maxPublicContentRows"', '"maxAdminLinkRows"', '"maxRequestBytes"', '"maxBatchRows"', '"label"')) {
		if ($topicXform -notmatch [regex]::Escape($needle)) {
			throw "content.topic instance.xform.json missing marker: $needle"
		}
	}
	$topicContracts = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/capability-pack/content.topic/contracts.json')
	foreach ($needle in @('"topic.content.sort"', '"topic.afterSort"', '"boundedSortLimit"', '"arrangeUi"', '"sortResultUi"', '"publicListLimit"', '"detailTopicLimit"', '"publicContentLimit"', '"adminLinkLimit"', '"adminLinkFilters"', '"adminStats"', '"adminStatsUi"', '"maxRequestBytes"', '"maxBatchRows"', '"requestLimit"', '"batchStatus"', '"batchStatusUi"', '"batchLimit"', '"statusValidation"', '"categoryBindIntegration"', '"accessIntegration"', '"topic.batch-status"', '"topic.stats"')) {
		if ($topicContracts -notmatch [regex]::Escape($needle)) {
			throw "content.topic contracts.json missing topic sort marker: $needle"
		}
	}
	Write-Output 'tag topic config wiring OK'
}

Invoke-Step 'access password hash wiring' {
	$mainTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_main.c.tpl')
	$abilityTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_ability.html.tpl')
	$contracts = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/capability-pack/content.access/contracts.json')
	foreach ($needle in @(
		'Managed_AccessPasswordEncodeLegacyXrt64',
		'Managed_AccessPasswordHashIterated',
		'xsha256i:',
		'passwordHashIterations',
		'Managed_AccessMaxPasswordInputBytes',
		'Managed_AccessMaxRequestBytes',
		'maxPasswordInputBytes',
		'content.access", "maxRequestBytes", 65536',
		'Managed_AbilityPackConfigBool("content.access", "allowQueryReadLevel", FALSE)',
		'Managed_AccessMaxReadLevel',
		'content.access", "maxReadLevel", 100',
		'access read level out of range',
		'access mode is invalid',
		'iStatus = iStatus ? 1 : 0',
		'authLevel',
		'__authLevel__',
		'access request body is too large',
		'access password maxLength is',
		'access rule save failed',
		'access rule not found',
		'access target not found',
		'Managed_CategoryRowExistsForBind',
		'SELECT 1 FROM content_category WHERE id=? AND delete_time=0 LIMIT 1',
		'xsha256:',
		'ServerHashPassword',
		'xrtMakeXIDS',
		'Managed_AccessLoadExistingPasswordHash',
		'password is required',
		'SELECT password_hash FROM content_access_rule WHERE id=? AND delete_time=0 LIMIT 1',
		'payRequired',
		'Managed_AccessPaidSessionAllowed',
		'paidSessionKey',
		'paidContentIds',
		'allowQueryReadLevel',
		'maxReadLevel',
		'orderValidated',
		'ruleMatched',
		'ruleSource',
		'content.access", "maxListRows"',
		'bCategoryPack ? sSqlWithCategory : sSqlWithoutCategory',
		'Managed_ReadTextQuery(objReq, "targetType"',
		'Managed_ReadTextQuery(objReq, "accessMode"',
		'Managed_ReadTextQuery(objReq, "status"',
		"(?='' OR (?='category' AND r.content_id<0) OR (?='content' AND r.content_id>0))",
		'(?<=0 OR ABS(r.content_id)=?)',
		'(?=2147483647 OR r.status=?)',
		'category ability pack is not enabled',
		'SELECT access_mode,required_read_level,password_hash,member_group_ids,price',
		'Managed_RequestAccessRuleStatsAdmin',
		'Managed_AppendAccessModeStat',
		'/access/rule/stats',
		'modeStats',
		'contentRuleCount',
		'categoryRuleCount'
	)) {
		if ($mainTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_main.c.tpl missing access password hash marker: $needle"
		}
	}
	if ($abilityTemplate -notmatch [regex]::Escape('public/login/level/group/password/paid/private')) {
		throw 'managed_ability.html.tpl missing access paid mode marker'
	}
	foreach ($needle in @(
		"configs['content.access'].listQuery",
		'accessFilterTargetType',
		'accessFilterTargetId',
		'accessFilterMode',
		'accessFilterStatus',
		'btnAccessFilter',
		'btnAccessFilterClear',
		'btnAccessStats',
		'showAccessStats',
		'modeStats',
		'contentRuleCount',
		'categoryRuleCount'
	)) {
		if ($abilityTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_ability.html.tpl missing access filter UI marker: $needle"
		}
	}
	if ($contracts -notmatch [regex]::Escape('ruleSourceOutput')) {
		throw 'content.access contracts.json missing rule source output marker'
	}
	foreach ($needle in @(
		'maxListRows',
		'maxRequestBytes',
		'listLimit',
		'adminRuleFilters',
		'passwordInputLimit',
		'requestLimit',
		'modeValidation',
		'statusValidation',
		'access.rule.stats',
		'adminStats',
		'adminStatsUi',
		'readLevelSource',
		'readLevelLimit'
	)) {
		if ($contracts -notmatch [regex]::Escape($needle)) {
			throw "content.access contracts.json missing list limit marker: $needle"
		}
	}
	Write-Output 'access password hash wiring OK'
}

Invoke-Step 'audit request ip wiring' {
	$mainTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_main.c.tpl')
	$auditContracts = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/capability-pack/content.audit-log/contracts.json')
	foreach ($needle in @(
		'Managed_AuditRequestIp',
		'xsReqRemote(objReq)',
		'bool Managed_AuditLogCore',
		'Managed_AuditLogWithRequest',
		'content.create',
		'category.create',
		'category.delete',
		'category.sort',
		'access_rule.save',
		'search.rebuild',
		'sitemap.refresh',
		'workflow.scheduled-run',
		'Managed_AuditLogWithRequest(pDb, "category"',
		'if ( bAuditLogPack )',
		'/audit-log/cleanup',
		'Managed_RequestAuditLogDetailAdmin',
		'/audit-log/detail',
		'Managed_RequestAuditLogStatsAdmin',
		'Managed_AppendAuditLogActionStat',
		'Managed_AppendAuditLogTargetTypeStat',
		'Managed_AppendAuditLogOperatorStat',
		'/audit-log/stats',
		'actionStats',
		'targetTypeStats',
		'operatorStats',
		'SELECT operator_type,operator_id,COUNT(*),COALESCE(MAX(create_time),0) FROM content_audit_log GROUP BY operator_type,operator_id',
		'audit log not found',
		'xrtParseJSON((str)sDetailJson',
		'xvoTableSetValue(tblData, "detail"',
		'Managed_AuditLogWithRequest(pDb, "media"',
		'Managed_AuditLogWithRequest(pDb, "form"',
		'Managed_AuditLogWithRequest(pDb, "import_job"',
		'content.audit-log", "maxListRows"',
		'content.audit-log", "maxCleanupRows"',
		'Managed_AuditLogMaxRequestBytes',
		'content.audit-log", "maxRequestBytes", 65536',
		'audit-log request body is too large',
		'audit cleanup failed',
		'Managed_ReadTextQuery(objReq, "targetType"',
		'Managed_ReadTextQuery(objReq, "action"',
		'Managed_ReadIntQuery(objReq, "targetId", 0)',
		"WHERE (?='' OR target_type=?) AND (?='' OR action=?) AND (?<=0 OR target_id=?) ORDER BY create_time DESC,id DESC LIMIT ?",
		'DELETE FROM content_audit_log WHERE id IN (SELECT id FROM content_audit_log WHERE create_time < ? ORDER BY create_time ASC,id ASC LIMIT ?)',
		'{\"deleted\":%d,\"beforeTime\":%lld,\"limit\":%d}',
		'Managed_BackgroundTaskCreate(pDb, "content.audit.cleanup"',
		'xvoTableSetInt(tblRet, "backgroundTaskId", 16, iBackgroundTaskId)',
		'xvoTableSetBool(tblRet, "queued", 6, iBackgroundTaskId > 0 ? TRUE : FALSE)'
	)) {
		if ($mainTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_main.c.tpl missing audit request ip marker: $needle"
		}
	}
	foreach ($needle in @(
		'maxListRows',
		'maxCleanupRows',
		'maxRequestBytes',
		'cleanupLimit',
		'requestLimit',
		'listFilters',
		'adminFilterUi',
		'listLimit',
		'audit-log.detail',
		'audit-log.stats',
		'adminStats',
		'operatorStats',
		'adminStatsUi',
		'cleanupResultUi',
		'detailView',
		'backgroundTaskAuditCleanup',
		'cleanupTaskUi',
		'content.audit.cleanup'
	)) {
		if ($auditContracts -notmatch [regex]::Escape($needle)) {
			throw "content.audit-log contracts.json missing list limit marker: $needle"
		}
	}
	if ($mainTemplate -match 'Managed_AuditLog\(pDb,[^\r\n]*objSession\);') {
		throw 'managed_main.c.tpl still has request audit calls without request ip'
	}
	$abilityTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_ability.html.tpl')
	foreach ($needle in @(
		'showAuditLogDetail',
		'lay-event="detail"',
		"configs['content.audit-log'].ops = 'detail'",
		"configs['content.audit-log'].listQuery",
		'auditTargetType',
		'auditTargetId',
		'auditAction',
		'btnAuditFilter',
		'btnAuditFilterClear',
		'renderAuditCleanupResult',
		'deletedCount',
		'backgroundTaskId',
		'queued',
		'maxCleanupRows',
		'auditStatTable',
		'auditOperatorTable',
		'actionStats',
		'targetTypeStats',
		'operatorStats',
		'操作者分布'
	)) {
		if ($abilityTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_ability.html.tpl missing audit detail UI marker: $needle"
		}
	}
	Write-Output 'audit request ip wiring OK'
}

Invoke-Step 'sitemap cache metadata wiring' {
	$mainTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_main.c.tpl')
	$sitemapContracts = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/capability-pack/content.sitemap/contracts.json')
	foreach ($needle in @(
		'sitemap/cache.json',
		'write-through',
		'Managed_SitemapEntryCount(pDb)',
		'cacheEntryCount',
		'cachePolicy',
		'Managed_SitemapCacheTtlSeconds',
		'Managed_SitemapCacheDirty',
		'Managed_SitemapLoadDirtyMeta',
		'Managed_SitemapAppendCacheFileStatus',
		'xrtFileGetSize(sFilePath)',
		'xrtFileGetChangeTime(sFilePath)',
		'xvoTableSetValue(tblData, "cacheFiles", 10',
		'XAdmin_PluginResourcePath(G_Handle, "static", "sitemap/dirty.json")',
		'xvoTableGetBool(tblDirty, "dirty", 5)',
		'xvoTableGetText(tblDirtyMeta, "reason", 6)',
		'xvoTableSetInt(tblData, "dirtyContentId", 14',
		'xvoTableSetInt(tblData, "dirtyUpdateTime", 15',
		'dirtyUpdateTimeText',
		'cacheTtlSeconds',
		'content.sitemap", "maxEntryListRows"',
		'content.sitemap", "maxRefreshRows"',
		'content.sitemap", "maxRssRows"',
		'Managed_ReadTextQuery(objReq, "contentId"',
		'Managed_ReadTextQuery(objReq, "status"',
		"AND (?<=0 OR id=?) AND (?=2147483647 OR status=?) ORDER BY update_time DESC,id DESC LIMIT ?",
		'xvoTableSetInt(tblRet, "contentIdFilter", 15, iContentId)',
		'maxEntryListRows',
		'maxRefreshRows',
		'maxRssRows',
		'Managed_SitemapRefreshLimitFromRequest',
		'Managed_RequestSitemapRefreshPlanAdmin',
		'/sitemap/refresh-plan',
		'writesData',
		'willTruncate',
		'eligibleCount',
		'cacheExpired',
		'Managed_SitemapMarkDirty',
		'sitemap/dirty.json',
		'cacheDirty',
		'xvoTableSetBool(tblRet, "cacheWritten", 12, bCacheWritten)',
		'sitemap cache write failed; dirty metadata remains for retry',
		'sitemap refresh transaction failed',
		'sitemap refresh commit failed',
		'Managed_SitemapRemoveEntry(pDb, iContentId)',
		'Managed_AccessCheckRule(pDb, iContentId, NULL, NULL, FALSE, NULL)',
		'(!bAccessPack) && Managed_SitemapReplyCache',
		'Managed_CategoryBindApplyToItem(pDb, tblItem)',
		'Managed_AccessCheckRule(pDb, xvoTableGetInt(tblItem, "id", 2), objReq, objSession, FALSE, NULL)',
		'xrtFileMove',
		'.tmp.',
		'ttlSeconds'
	)) {
		if ($mainTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_main.c.tpl missing sitemap cache metadata marker: $needle"
		}
	}
	$abilityTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_ability.html.tpl')
	foreach ($needle in @(
		'function renderSitemapStats',
		'sitemapStatsTable',
		'cacheEntryCount',
		'cacheTtlSeconds',
		'dirtyFile',
		'cacheDirty',
		'dirtyReason',
		'dirtyContentId',
		'dirtyUpdateTimeText',
		'cacheFiles',
		'sizeBytes',
		'cacheExpired',
		'btnSitemapRefreshPlan',
		'showSitemapRefreshPlan',
		'renderSitemapRefreshPlan',
		'renderSitemapRefreshResult',
		'willRefreshRows',
		'writesData',
		'btnSitemapFilter',
		'btnSitemapFilterClear',
		'sitemapContentIdFilter',
		'sitemapStatusFilter',
		'sitemapRefreshLimit',
		'sitemapRefreshQuery',
		"['contentId', abilityListFilters.sitemapContentId]"
	)) {
		if ($abilityTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_ability.html.tpl missing sitemap stats UI marker: $needle"
		}
	}
	foreach ($needle in @('"siteUrl"', '"cacheTtlSeconds"', '"maxEntryListRows"', '"maxRefreshRows"', '"maxRssRows"', '"sitemap.refresh-plan"', 'entryListLimit', 'entryListFilters', 'refreshPlan', 'refreshPlanUi', 'refreshLimit', 'refreshResultUi', 'refreshUiLimit', 'rssLimit', 'dirtyMetadata', 'dirtyMetadataDetail', 'cacheFileDiagnostics', 'categoryBindIntegration', 'accessIntegration')) {
		if ($sitemapContracts -notmatch [regex]::Escape($needle)) {
			throw "content.sitemap contracts.json missing config marker: $needle"
		}
	}
	Write-Output 'sitemap cache metadata wiring OK'
}

Invoke-Step 'static access guard wiring' {
	$mainTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_main.c.tpl')
	$abilityTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_ability.html.tpl')
	$staticXform = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/capability-pack/content.static/instance.xform.json')
	foreach ($needle in @(
		'Managed_StaticRenderRestrictedShellToFile',
		'restricted shell generated',
		'noindex,nofollow',
		'Managed_StaticCreateTask(pDb, iRuleId, "content", iTargetId',
		'content.static", "maxGenerateRequestBytes", 8192',
		'static generate request body is too large',
		'Managed_StaticMaxRequestBytes',
		'content.static", "maxRequestBytes", 65536',
		'static request body is too large',
		'static rule save failed',
		'static rule not found',
		'strlen(sRel) > 240',
		'strlen(sDir) > 120',
		'!Managed_AccessCheckRule(pDb, iTargetId, NULL, NULL, FALSE, NULL)',
		'autoGenerateRuleLimit',
		'SELECT id FROM static_rule WHERE status=1 ORDER BY id ASC LIMIT ?',
		'if ( iAutoRuleLimit > 200 ) iAutoRuleLimit = 200',
		'content.static", "maxListRows"',
		'content.static", "maxCleanRows"',
		"static_rule WHERE (?='' OR path_pattern LIKE ?) AND (?=999 OR status=?) ORDER BY status DESC,id DESC LIMIT ?",
		'xvoTableSetText(tblRet, "pathPatternFilter", 17, sPathPattern',
		'static_task WHERE (?=999 OR status=?) AND (?<=0 OR rule_id=?) AND (?<=0 OR (target_type=''content'' AND target_id=?)) ORDER BY create_time DESC,id DESC LIMIT ?',
		'xvoTableSetInt(tblRet, "statusFilter", 12, iStatusFilter)',
		'xvoTableSetInt(tblRet, "ruleIdFilter", 12, iRuleIdFilter)',
		"static_artifact WHERE (?<=0 OR (target_type='content' AND target_id=?)) AND (?='' OR path LIKE '%' || ? || '%') ORDER BY update_time DESC,id DESC LIMIT ?",
		'xvoTableSetInt(tblRet, "targetIdFilter", 14, iTargetId)',
		'xvoTableSetText(tblRet, "pathFilter", 10, sPath',
		'DELETE FROM static_artifact WHERE id IN (SELECT id FROM static_artifact',
		'ORDER BY id ASC LIMIT ?)',
		'UPDATE static_task SET status=-1,message=?,finish_time=? WHERE id=?',
		'artifact save failed',
		'xvoTableSetInt(tblRet, "cleaned", 7, iCleaned)',
		'xvoTableSetInt(tblRet, "limit", 5, iLimit)',
		'xvoTableSetInt(tblRet, "maxCleanRows", 12, iMaxCleanRows)',
		'content.static.clean',
		'xvoTableSetInt(tblRet, "backgroundTaskId", 16, iBackgroundTaskId)',
		'xvoTableSetValue(tblRet, "rows", 4, arrRows, TRUE)',
		'xvoTableSetInt(tblRow, "artifactId", 10',
		'xvoTableSetBool(tblRow, "fileDeleteAttempted", 19',
		'xvoTableSetInt(tblRet, "targetId", 8, iTargetId)',
		'xvoTableSetInt(tblRet, "ruleId", 6, iRuleId)',
		'/static/task/status',
		'Managed_RequestStaticTaskStatusAdmin',
		'/static/task/retry',
		'Managed_RequestStaticTaskRetryAdmin',
		'/static/task/retry-failed',
		'Managed_RequestStaticTaskRetryFailedAdmin',
		'/static/stats',
		'Managed_RequestStaticStatsAdmin',
		'Managed_AppendStaticStatusStat',
		'ruleStatusStats',
		'taskStatusStats',
		'artifactTargetCount',
		'content.static", "maxTaskRetryRows", 20',
		'static.task.retryFailed',
		'xvoTableSetInt(tblRet, "maxTaskRetryRows", 16, iMaxRetryRows)',
		'iStatus = iStatus ? 1 : 0',
		'static.task.retry'
	)) {
		if ($mainTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_main.c.tpl missing static access guard marker: $needle"
		}
	}
	foreach ($needle in @(
		'btnStaticClean',
		'btnStaticRuleFilter',
		'btnStaticRuleFilterClear',
		'staticRulePathFilter',
		'staticRuleStatusFilter',
		"['pathPattern', abilityListFilters.staticRulePathPattern]",
		'btnStaticArtifactFilter',
		'btnStaticArtifactFilterClear',
		'staticArtifactTargetIdFilter',
		'staticArtifactPathFilter',
		"['targetId', abilityListFilters.staticArtifactTargetId]",
		"['path', abilityListFilters.staticArtifactPath]",
		'lay-event="retry"',
		'/static/task/retry',
		'staticCleanForm',
		'staticCleanResult',
		'staticTaskStatusFilter',
		'staticTaskRuleIdFilter',
		'staticTaskTargetIdFilter',
		'btnStaticTaskFilter',
		'btnStaticRetryFailed',
		'btnStaticStats',
		'showStaticStats',
		'renderStaticStats',
		'renderStaticStatusRows',
		'renderStaticGenerateResult',
		'renderStaticCleanResult',
		'fileDeleteAttempted',
		'content.static.clean',
		'renderStaticRetryFailedResult',
		'renderStaticRulePreviewResult',
		'renderRouteRuleRefreshResult',
		'runRouteRuleRefresh',
		'runStaticRetryFailed',
		'staticRetryFailedLimit',
		'staticTaskRetryFailedResult',
		'ruleStatusStats',
		'taskStatusStats',
		'artifactTargetCount',
		'/static/task/retry-failed',
		'abilityListFilters.staticTaskStatus',
		"listQuery:function(){return queryFromPairs([['status', abilityListFilters.staticTaskStatus], ['ruleId', abilityListFilters.staticTaskRuleId], ['targetId', abilityListFilters.staticTaskTargetId]])",
		'/static/clean'
	)) {
		if ($abilityTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_ability.html.tpl missing static cleanup UI marker: $needle"
		}
	}
	foreach ($needle in @('"autoGenerateRuleLimit"', '"maxListRows"', '"maxCleanRows"', '"maxTaskRetryRows"', '"maxGenerateRequestBytes"', '"maxRequestBytes"')) {
		if ($staticXform -notmatch [regex]::Escape($needle)) {
			throw "content.static instance.xform.json missing $needle"
		}
	}
	$staticContracts = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/capability-pack/content.static/contracts.json')
	foreach ($needle in @('"maxRequestBytes"', '"adminRequestLimit"', '"static.rule.preview"', '"rulePreview"', '"rulePreviewUi"', '"static.task.status"', '"static.task.retry"', '"static.task.retry-failed"', '"static.stats"', '"adminStats"', '"adminStatsUi"', '"generateResultUi"', '"cleanResultUi"', '"cleanRows"', '"backgroundTaskStaticClean"', '"taskPersistenceStatus"', '"ruleStatusValidation"', '"taskRetry"', '"taskRetryFailed"', '"taskRetryFailedUi"', '"taskStatusFilter"', '"taskRuleTargetFilter"', '"artifactTargetFilter"', '"artifactPathFilter"', '"ruleListFilters"', '"unifiedRulePlan"', '"routeRulePlanApi"', '"route-rule.plan"', '"routeRuleValidateApi"', '"route-rule.validate"', '"routeRuleListApi"', '"route-rule.list"', '"routeRuleSaveApi"', '"route-rule.save"', '"routeRuleStatusApi"', '"route-rule.status"', '"routeRuleSortApi"', '"route-rule.sort"', '"routeRuleStatsApi"', '"route-rule.stats"', '"routeRuleRefreshApi"', '"route-rule.refresh"', '"routeRulePlanUi"', '"routeRuleValidateUi"', '"routeRuleStatsUi"', '"routeRuleRefreshUi"', '"routeRuleEditDialogUi"', '"routeRuleRuntimeRefresh"', '"independentRouteRuleStore"', '"routeRuleEditableContract"', '"routeRuleListFilters"', '"sourcePack"', '"keyword"')) {
		if ($staticContracts -notmatch [regex]::Escape($needle)) {
			throw "content.static contracts.json missing admin request boundary marker: $needle"
		}
	}
	Write-Output 'static access guard wiring OK'
}

Invoke-Step 'import export paging UI wiring' {
	$abilityTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_ability.html.tpl')
	foreach ($needle in @(
		'importExportLastExport',
		'runImportExportExport',
		'renderImportExportExportResult',
		'renderImportExportExportRows',
		'btnExportJsonNext',
		'btnImportExportFieldPlan',
		'runImportExportFieldPlan',
		'renderImportExportFieldPlanResult',
		'renderImportExportImportResult',
		'renderImportExportImportRows',
		'renderImportExportChunkResult',
		'btnImportExportStats',
		'showImportExportStats',
		'btnImportStage',
		'runImportStage',
		'/import-export/import/stage',
		'/import-export/stats',
		'importExportResult',
		'statusTable',
		'importStatus',
		'exportStatus',
		'importFailureSamples',
		'failureSampleLimit',
		'failureTable',
		'importFailCount',
		'nextOffset',
		'runImportExportChunked',
		'importExportChunkSize',
		'btnImportChunkCommit',
		'importExportJobStatus',
		'btnImportExportJobFilter',
		'btnImportExportJobFilterClear',
		'abilityListFilters.importExportJobStatus',
		"listQuery:function(){return queryFromPairs([['status', abilityListFilters.importExportJobStatus]])"
	)) {
		if ($abilityTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_ability.html.tpl missing import/export paging UI marker: $needle"
		}
	}
	$mainTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_main.c.tpl')
	$importContracts = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/capability-pack/content.import-export/contracts.json')
	foreach ($needle in @(
		'Managed_ImportBatchSizeAllowed',
		'Managed_ImportMaxBatchRows',
		'content.import-export',
		'maxBatchRows',
		'maxExportRows',
		'maxJobRows',
		'Managed_ImportExportMaxRequestBytes',
		'content.import-export", "maxRequestBytes", 1048576',
		'import-export request body is too large',
		'Managed_AbilityPackConfigInt("content.import-export", "maxExportRows", 1000)',
		'Managed_AbilityPackConfigInt("content.import-export", "maxJobRows", 200)',
		'Managed_ImportExportBuildFieldPlan',
		'Managed_RequestImportExportFieldPlanAdmin',
		'Managed_RequestImportExportStatsAdmin',
		'Managed_RequestImportStageAdmin',
		'Managed_AppendImportExportStatusStat',
		'/import-export/import/stage',
		'content.import.stage',
		'/import-export/field-plan',
		'/import-export/stats',
		'fieldPlan',
		'importStatus',
		'exportStatus',
		'importFailureSamples',
		'failureSampleLimit',
		"content_import_job WHERE fail_count>0 ORDER BY finish_time DESC,id DESC LIMIT ?",
		'ignoredFields',
		"content_import_job WHERE (?='' OR status=?) ORDER BY create_time DESC,id DESC LIMIT ?",
		"content_export_job WHERE (?='' OR status=?) ORDER BY create_time DESC,id DESC LIMIT ?",
		'xvoTableSetText(tblRet, "statusFilter", 12, (str)sStatusFilter, 0, FALSE)',
		'defaultCapped',
		'import batch size must be <= %d rows',
		'conflictMode must be insert, update, or skip',
		'sJobStatus = (iFail <= 0) ? "imported" : ((iSuccess > 0) ? "partial_failed" : "failed")',
		'xvoTableSetText(tblRet, "jobStatus", 9',
		'import commit transaction failed',
		'import commit transaction commit failed',
		'bCategoryPack = Managed_AbilityPackMounted("content.category")',
		'if ( !bCategoryPack ) iCategoryId = 0',
		'if ( bCategoryPack )',
		'Managed_CategoryBindSet(pDb, iContentId, iCategoryId > 0 ? iCategoryId : 0, iNow)',
		'category bind sync failed',
		'if ( bOk && !Managed_RevisionSnapshot',
		'if ( bOk && !Managed_ContentSyncDerivedData',
		'Managed_CategoryBindApplyToItem(pDb, tblItem)',
		'xvoTableSetInt(tblData, "categoryId", 10, xvoTableGetInt(tblItem, "categoryId", 10))',
		'xvoTableSetBool(tblRet, "truncated", 9, bTruncated)',
		'xvoTableSetInt(tblRet, "maxBatchRows", 12, iMaxRows)',
		'xvoTableSetBool(tblRet, "jobSaved", 8, iJobId > 0 ? TRUE : FALSE)',
		'import preview job was not saved',
		'import job was not saved',
		'export job was not saved',
		'admin UI runs chunked preview/commit calls'
	)) {
		if ($mainTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_main.c.tpl missing import/export bounded batch marker: $needle"
		}
	}
	foreach ($needle in @('defaultExportLimit', 'jobListLimit', 'jobStats', 'jobStatsUi', 'failureSamples', 'jobStatusFilter', 'importResultStatus', 'exportResultUi', 'importResultUi', 'chunkResultUi', 'replayLimit', 'fieldPlan', 'fieldPlanUi', 'categoryBindIntegration', 'categoryBindImportSync', 'maxRequestBytes', 'requestLimit', 'stagedUpload', 'stagedUploadUi', 'backgroundTaskImportStage', 'import-export.import.stage', 'import-export.stats')) {
		if ($importContracts -notmatch [regex]::Escape($needle)) {
			throw "content.import-export contracts.json missing marker: $needle"
		}
	}
	Write-Output 'import/export paging UI wiring OK'
}

Invoke-Step 'related rebuild bounded UI wiring' {
	$abilityTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_ability.html.tpl')
	$mainTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_main.c.tpl')
	$relatedContracts = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/capability-pack/content.related/contracts.json')
	foreach ($needle in @(
		'relatedRebuildResult',
		'renderRelatedRebuildResult',
		'content.related.publishRefreshLimit',
		'content.related.ruleLimit',
		'btnRelatedRebuild',
		'btnRelatedRulePreview',
		'btnRelatedStats',
		'btnRelatedFilter',
		'btnRelatedFilterClear',
		'relatedFilterSourceContentId',
		'relatedFilterRelatedContentId',
		'relatedFilterRelationType',
		'relatedFilterStatus',
		"['sourceContentId', abilityListFilters.relatedSourceContentId]",
		'previewRelatedRules',
		'renderRelatedRulePreview',
		'showRelatedStats',
		'/related/rule/preview',
		'/related/stats',
		'ruleConfig',
		'ruleFormula',
		'if(limit > 20) limit = 20',
		'min="1" max="20"'
	)) {
		if ($abilityTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_ability.html.tpl missing related rebuild bounded UI marker: $needle"
		}
	}
	foreach ($needle in @(
		"NOT EXISTS (SELECT 1 FROM content_related m",
		"m.relation_type='manual'",
		'Managed_RelatedSyncPeers',
		'Managed_RelatedRefreshPublishPeers',
		'publishRefreshLimit',
		'maxPublicRelated',
		'maxAdminListRows',
		'maxRebuildSources',
		'Managed_RelatedMaxRequestBytes',
		'content.related", "maxRequestBytes", 65536',
		'related request body is too large',
		'SELECT i.id,cb.category_id FROM content_category_bind cb INNER JOIN content_item i ON i.id=cb.content_id WHERE cb.status=1 AND cb.delete_time=0 AND i.delete_time=0 AND i.is_draft=0 AND i.status>=1 ORDER BY i.update_time DESC,i.id DESC LIMIT ?',
		'Publish-time fan-out is bounded',
		'Managed_RelatedRuleWeight',
		'Managed_RelatedSharedTagCount',
		'Managed_RelatedSharedTopicCount',
		'Managed_RequestRelatedRulePreviewAdmin',
		'Managed_RequestRelatedStatsAdmin',
		'Managed_AppendRelatedTypeStat',
		'Managed_ReadTextQuery(objReq, "sourceContentId"',
		'Managed_ReadTextQuery(objReq, "relatedContentId"',
		'Managed_ReadTextQuery(objReq, "relationType"',
		"(?<=0 OR r.source_content_id=?) AND (?<=0 OR r.related_content_id=?) AND (?='' OR r.relation_type=?) AND (?=2147483647 OR r.status=?)",
		'route.path = "/admin/api/plugin/{{PLUGIN_XID}}/related/rule/preview"',
		'route.path = "/admin/api/plugin/{{PLUGIN_XID}}/related/stats"',
		'typeStats',
		'ruleConfig',
		'publishRefreshLimit',
		'maxRebuildSources',
		'manualCount',
		'ruleCount',
		'candidateCount',
		'categoryScore',
		'sharedTagCount',
		'sharedTopicCount',
		'ruleFormula',
		'reasonText',
		'writeMode',
		'writesData',
		'categoryWeight',
		'tagWeight',
		'topicWeight',
		'related save failed',
		'related item not found',
		'related content not found',
		'related rebuild transaction failed',
		'related rebuild write failed',
		'related rebuild commit failed',
		'Managed_CategoryBindApplyToItem(pDb, tblItem)',
		'Managed_AccessCheckRule(pDb, xvoTableGetInt(tblItem, "id", 2), objReq, objSession, FALSE, NULL)',
		'content_tag a INNER JOIN content_tag b',
		'topic_content a INNER JOIN topic_content b'
	)) {
		if ($mainTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_main.c.tpl missing related de-dup marker: $needle"
		}
	}
	foreach ($needle in @(
		'maxPublicRelated',
		'publicOutputLimit',
		'maxAdminListRows',
		'adminListLimit',
		'maxRebuildSources',
		'rebuildSourceLimit',
		'maxRequestBytes',
		'requestLimit',
		'accessIntegration',
		'categoryBindIntegration',
		'rulePreview',
		'rulePreviewReadOnly',
		'rebuildResultUi',
		'related.rule.preview',
		'related.stats',
		'adminStats',
		'adminStatsUi',
		'ruleConfigDiagnostics',
		'statusValidation',
		'adminListFilters'
	)) {
		if ($relatedContracts -notmatch [regex]::Escape($needle)) {
			throw "content.related contracts.json missing public output marker: $needle"
		}
	}
	Write-Output 'related rebuild bounded UI wiring OK'
}

Invoke-Step 'search probe UI wiring' {
	$abilityTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_ability.html.tpl')
	$mainTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_main.c.tpl')
	$searchContracts = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/capability-pack/content.search/contracts.json')
	foreach ($needle in @(
		'runSearchProbe',
		'runSearchExplain',
		'renderSearchProbeResult',
		'renderSearchExplainResult',
		'renderSearchRebuildResult',
		'searchProbeQuery',
		'searchProbeResult',
		'btnSearchFilter',
		'btnSearchFilterClear',
		'btnSearchStats',
		'renderSearchStats',
		'missingCount',
		'indexCount',
		'searchCategoryId',
		'abilityListFilters.searchCategoryId',
		"['q', abilityListFilters.searchQuery]",
		"['categoryId', abilityListFilters.searchCategoryId]",
		'btnSearchProbe',
		'btnSearchExplain',
		'topResults',
		'queryTooShort',
		'normalizedQuery',
		'searchScore',
		'searchBaseScore',
		'exactPhraseScore',
		'termCoverageScore',
		'termCoverageWeight',
		'cjkBigramScore',
		'cjkBigramWeight',
		'freshnessScore',
		'freshnessWeight',
		'freshnessWindowDays',
		'searchRebuildOffset',
		'searchRebuildLimit',
		'/search/rebuild?offset=',
		'data.hasMore',
		'maxRebuildRows',
		'backgroundTaskId',
		'data.backgroundTaskId',
		'content.search.rebuild',
		'search query is required',
		'if(limit > 20) limit = 20'
	)) {
		if ($abilityTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_ability.html.tpl missing search probe UI marker: $needle"
		}
	}
	foreach ($needle in @(
		'Managed_BuildSearchLikePattern',
		'Managed_BuildSearchPrefixLikePattern',
		'Managed_SearchNormalizeQuery',
		'Managed_SearchIsDelimiter',
		'Managed_SearchDelimiterBytes',
		'Managed_SearchUtf8CjkCharBytes',
		'Managed_SearchQueryHasCjk',
		'content_category_bind cb WHERE cb.content_id=i.id AND cb.category_id=?',
		'categoryFilter',
		'xvoTableSetInt(tblRet, "categoryId", 10',
		'xvoTableSetBool(tblRet, "categoryFilter", 14',
		'normalizedQuery',
		"(ch == ' ') || (ch == '\t')",
		'(s[0] == 0xEF) && (s[1] == 0xBC)',
		'bPrevCjk && bCjk',
		"(*p == '%') || (*p == '_') || (*p == '~')",
		"LIKE ? ESCAPE '~'",
		'cjkLooseLike',
		'Managed_SearchBuildSnippet',
		'Managed_TextFindIgnoreCase',
		'Managed_TextContainsSearchTerms',
		'Managed_SearchFindFirstTerm',
		'iHit = Managed_SearchFindFirstTerm(sText, sQuery)',
		'Managed_SearchWeight',
		'Managed_SearchTermCoverageCount',
		'Managed_SearchTermCoverageScore',
		'Managed_SearchCjkBigramCoverageCount',
		'Managed_SearchCjkBigramScore',
		'Managed_SearchExactPhraseScore',
		'Managed_SearchFreshnessScore',
		'Managed_SearchAppendRankedResult',
		'content.search',
		'titleWeight',
		'exactTitleWeight',
		'idx.title=? COLLATE NOCASE',
		'prefixTitleWeight',
		'sPrefixLike',
		'keywordWeight',
		'summaryWeight',
		'bodyWeight',
		'exactPhraseWeight',
		'exactPhraseScore',
		'termCoverageWeight',
		'termCoverageScore',
		'cjkBigramWeight',
		'xvoTableSetInt(tblItem, "cjkBigramScore", 14, iCjkBigramScore)',
		'freshnessWeight',
		'freshnessWindowDays',
		'xvoTableSetInt(tblItem, "freshnessScore", 14, iFreshnessScore)',
		'searchBaseScore',
		'minQueryLength',
		'maxResultLimit',
		'maxRebuildRows',
		'SELECT id,title,status,payload_json,is_draft,create_time,update_time,category_id FROM content_item WHERE delete_time=0 ORDER BY id ASC LIMIT ? OFFSET ?',
		'search rebuild transaction failed',
		'search rebuild commit failed',
		'Managed_BackgroundTaskCreate(pDb, "content.search.rebuild"',
		'xvoTableSetInt(tblRet, "backgroundTaskId", 16, iBackgroundTaskId)',
		'xvoTableSetBool(tblRet, "queued", 6, iBackgroundTaskId > 0 ? TRUE : FALSE)',
		'bTaskPack = bStaticPack || bSitemapPack || bImportExportPack || bSearchPack || bFormPack || bAuditLogPack',
		'Managed_CategoryBindApplyToItem(pDb, tblItem)',
		'hasMore',
		'nextOffset',
		'queryTooShort',
		'iCount = (int)xvoArrayItemCount(arrList)',
		'xrtCopyStr((str)(sText + iStart), 240)'
	)) {
		if ($mainTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_main.c.tpl missing search query token marker: $needle"
		}
	}
	$searchBody = [regex]::Match($mainTemplate, '(?s)void Managed_RequestSearchCommon.*?void Managed_RequestSearchPublic').Value
	if ($searchBody -match [regex]::Escape('Managed_RequestListCommon(objResp, objReq, objSession, bAdmin, 0);') -and $searchBody -match [regex]::Escape('if ( xvoArrayItemCount(arrList) <= 0 )')) {
		throw 'Managed_RequestSearchCommon must return an empty result for non-empty no-hit searches, not fall back to content list'
	}
	if ($searchContracts -notmatch [regex]::Escape('"configKeys"')) {
		throw 'content.search contracts.json missing configKeys marker'
	}
	foreach ($needle in @(
		'minQueryLength',
		'maxResultLimit',
		'maxRebuildRows',
		'rebuildLimit',
		'queryGuards',
		'adminListFilters',
		'adminStatsUi',
		'rankingExplain',
		'rankingExplainUi',
		'probeResultUi',
		'rebuildResultUi',
		'rebuildBackgroundTask',
		'rebuildTaskUi',
		'prefixTitleWeight',
		'exactPhraseWeight',
		'exactPhraseScore',
		'termCoverageWeight',
		'termCoverage',
		'cjkBigramWeight',
		'cjkBigramScore',
		'categoryBindIntegration',
		'accessIntegration',
		'cjkPunctuation',
		'cjkLooseLike',
		'categoryFilter'
	)) {
		if ($searchContracts -notmatch [regex]::Escape($needle)) {
			throw "content.search contracts.json missing query guard marker: $needle"
		}
	}
	Write-Output 'search probe UI wiring OK'
}

Invoke-Step 'metric stats wiring' {
	$abilityTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_ability.html.tpl')
	$dashboardTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_dashboard.html.tpl')
	$mainTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_main.c.tpl')
	$generator = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/script/content/content_generator.h')
	$generation = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/script/content/content_generation.h')
	$likeXform = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/capability-pack/content.like/instance.xform.json')
	$likeContracts = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/capability-pack/content.like/contracts.json')
	$viewContracts = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/capability-pack/content.view-stat/contracts.json')
	foreach ($needle in @(
		'Managed_RequestLikeStatsAdmin',
		'/like/stats',
		'SELECT COUNT(*),COALESCE(SUM(like_count),0)',
		'Managed_RequestViewStatsAdmin',
		'/view/stats',
		'SELECT COUNT(*),COALESCE(SUM(view_count),0)',
		'route.path = "/api/plugin/{{PLUGIN_XID}}/view/status"',
		'route.path = "/api/plugin/{{PLUGIN_XID}}/view/count"',
		'route.path = "/api/plugin/{{PLUGIN_XID}}/view/detail"',
		'xvoTableSetInt(tblRet, "viewCount", 9, iViewCount)',
		'xvoTableSetInt(tblRet, "uniqueViewCount", 15, iUniqueCount)',
		'Managed_MetricKeySafe',
		'Managed_LikeActorKey(tblBody, xsReqRemote(objReq))',
		'sIp = !Managed_IsBlank(xsReqRemote(objReq)) ? xsReqRemote(objReq) : sBodyIp',
		'actor key is invalid',
		'like save failed',
		'like record not found',
		'xvoTableSetBool(tblRet, "changed", 7, bUpdated)',
		'Managed_LikeMaxRequestBytes',
		'like request body is too large',
		'Managed_PublicContentVisibleForAccess(pDb, iContentId, objReq, objSession)',
		'Managed_RequestLikeSetStatusAdmin',
		'visitor key is invalid',
		'sVisitorKey = Managed_ViewVisitorKey(tblBody)',
		'Managed_ViewMaxRequestBytes',
		'view record request body is too large',
		'view_log(content_id,visitor_key,ip,referer,user_agent,create_time)',
		'view log save failed',
		'view counter save failed',
		'bCounterSaved = Managed_ViewUpdateCounter(pDb, iContentId, bUnique)',
		'bDailySaved = Managed_ViewUpdateDaily(pDb, iContentId, bUnique)',
		'Managed_ViewUniqueWindowSeconds',
		'uniqueWindowSeconds',
		'content.like", "maxAdminListRows"',
		'content.view-stat", "maxRankRows"',
		'content.view-stat", "maxAdminListRows"',
		'like_counter ORDER BY like_count DESC, update_time DESC LIMIT ?',
		'view_counter ORDER BY view_count DESC, content_id ASC LIMIT ?',
		'view_counter ORDER BY view_count DESC,last_view_time DESC LIMIT ?',
		'view_daily_stat ORDER BY stat_date DESC,view_count DESC LIMIT ?',
		'create_time>=?',
		'Managed_RequestDashboardView',
		'generated/dashboard.html',
		'/admin/view/plugin/{{PLUGIN_XID}}/dashboard'
	)) {
		if ($mainTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_main.c.tpl missing metric stats marker: $needle"
		}
	}
	$likeListBody = [regex]::Match($mainTemplate, '(?s)void Managed_RequestLikeListAdmin.*?void Managed_RequestLikeCounterListAdmin').Value
	if ($likeListBody -match 'Managed_AbilityPackConfigInt\("content\.view-stat", "maxAdminListRows"') {
		throw 'Managed_RequestLikeListAdmin must read content.like maxAdminListRows, not content.view-stat'
	}
	foreach ($needle in @(
		'showMetricStats',
		'btnLikeStats',
		'btnViewStats',
		'/like/stats',
		'/view/stats',
		'<th style="width:220px">'
	)) {
		if ($abilityTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_ability.html.tpl missing metric stats marker: $needle"
		}
	}
	foreach ($needle in @(
		'managed_dashboard.html.tpl',
		'Content_BuildManagedDashboardHtml',
		'generated/dashboard.html',
		'bMetricPack'
	)) {
		if (($generator + $generation) -notmatch [regex]::Escape($needle)) {
			throw "content generator missing dashboard marker: $needle"
		}
	}
	foreach ($needle in @(
		'/like/stats',
		'/view/stats',
		'/view/daily/list',
		'TopTable_{{PLUGIN_DOM_ID_BASE}}',
		'Cards_{{PLUGIN_DOM_ID_BASE}}',
		'TrendTable_{{PLUGIN_DOM_ID_BASE}}',
		'TrendChart_{{PLUGIN_DOM_ID_BASE}}',
		'dailyTrendRows',
		'aggregateTrendByDate',
		'drawTrendChart',
		'ctx.arc'
	)) {
		if ($dashboardTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_dashboard.html.tpl missing metric dashboard marker: $needle"
		}
	}
	foreach ($needle in @('allowGuest', 'dedup', 'maxAdminListRows', 'maxRequestBytes', 'requestLimit', 'adminListLimit', 'adminStats', 'adminStatsUi', 'accessIntegration')) {
		if (($likeXform + $likeContracts) -notmatch [regex]::Escape($needle)) {
			throw "content.like config marker missing: $needle"
		}
	}
	foreach ($needle in @('rankEnabled', 'configKeys', 'maxRankRows', 'maxAdminListRows', 'maxRequestBytes', 'requestLimit', 'rankLimit', 'adminListLimit', 'adminStats', 'adminStatsUi', 'accessIntegration')) {
		if ($viewContracts -notmatch [regex]::Escape($needle)) {
			throw "content.view-stat contract marker missing: $needle"
		}
	}
	Write-Output 'metric stats wiring OK'
}

Invoke-Step 'workflow due-run bounded UI wiring' {
	$abilityTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_ability.html.tpl')
	$mainTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_main.c.tpl')
	$workflowContracts = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/capability-pack/content.workflow/contracts.json')
	$utf8 = [System.Text.Encoding]::UTF8
	$todoTitle = $utf8.GetString([byte[]]@(0xe5,0xae,0xa1,0xe6,0xa0,0xb8,0xe5,0xbe,0x85,0xe5,0x8a,0x9e))
	foreach ($needle in @(
		'workflowScheduledLimit',
		'workflowScheduledResult',
		'btnWorkflowStats',
		'showWorkflowStats',
		'renderWorkflowStats',
		'renderWorkflowActionResult',
		'renderWorkflowActionStats',
		'renderWorkflowPlanRows',
		'renderWorkflowScheduledResult',
		'actionStats',
		'notificationActionStats',
		'approvalConfig',
		'scheduledDue',
		'workflowPlan',
		'/workflow/stats',
		'btnWorkflowRunScheduled',
		'content.workflow',
		'if(limit > 200) limit = 200',
		'min="1" max="200"',
		'/workflow/todo/list',
		'/workflow/notification/list',
		'btnWorkflowFilter',
		'btnWorkflowFilterClear',
		'abilityListFilters.workflowContentId',
		'abilityListFilters.workflowAssigneeId',
		'abilityListFilters.workflowAction',
		'abilityListFilters.workflowStatus',
		'abilityListFilters.workflowReadStatus',
		'workflowAction',
		'workflowStatus',
		'workflowReadStatus',
		'queryFromPairs',
		$todoTitle,
		'notifications',
		'lastAction',
		'lastReason'
	)) {
		if ($abilityTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_ability.html.tpl missing workflow due-run bounded UI marker: $needle"
		}
	}
	foreach ($needle in @(
		'Managed_RequestWorkflowTodoListAdmin',
		'Managed_RequestWorkflowNotificationListAdmin',
		'Managed_RequestWorkflowStatsAdmin',
		'Managed_WorkflowCountScheduledDue',
		'Managed_AppendWorkflowActionStat',
		'Managed_WorkflowBuildPlan',
		'Managed_WorkflowAppendNode',
		'Managed_WorkflowAppendTransition',
		'/workflow/stats',
		'actionStats',
		'notificationActionStats',
		'approvalConfig',
		'scheduledDue',
		'workflowPlan',
		'workflow-v1-requiredApprovals-compatible',
		'compatRequiredApprovals',
		'assigneePolicy',
		'scannedDraftCount',
		'diagnosticMode',
		'unreadNotificationCount',
		'Managed_WorkflowNotify',
		'content_workflow_notification',
		'Managed_WorkflowTransitionAllowed',
		'Managed_MediaSyncRefs(pDb, iContentId, tblData, iNow)',
		'content must be waiting for review before approve',
		'published content cannot be scheduled',
		'/workflow/todo/list',
		'content_workflow_log l ON l.id=',
		'c.delete_time=0 AND (c.is_draft=1 OR c.status=0)',
		"(?<=0 OR content_id=?) AND (?<=0 OR assignee_id=?) AND (?='' OR action=?) ORDER BY id DESC LIMIT ?",
		"(?<=0 OR COALESCE(l.assignee_id,0)=?) AND (?=2147483647 OR c.status=?)",
		"content_workflow_notification WHERE (?<=0 OR content_id=?) AND (?<=0 OR assignee_id=?) AND (?='' OR action=?) AND (?=2147483647 OR read_status=?)",
		'content.workflow", "maxListRows"',
		'content.workflow", "maxReasonLength"',
		'Managed_WorkflowRequiredApprovals',
		'Managed_WorkflowApprovalCount',
		'Managed_WorkflowRequireDistinctApprovers',
		'Managed_WorkflowOperatorAlreadyApproved',
		'Managed_SessionOperatorId',
		'operatorId',
		'content.workflow", "requiredApprovals", 1',
		'content.workflow", "requireDistinctApprovers", FALSE',
		'operator already approved in current workflow round',
		"action='approve'",
		'approvalPending',
		'Managed_WorkflowMaxRequestBytes',
		'content.workflow", "maxRequestBytes", 65536',
		'workflow request body is too large',
		'workflow reason maxLength is',
		'(sqlite3_step(stmtUpdate) == SQLITE_DONE) && (sqlite3_changes(pDb) > 0)',
		'workflow scheduled transaction failed',
		'workflow scheduled commit failed',
		'LIMIT ?',
		'maxListRows'
	)) {
		if ($mainTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_main.c.tpl missing workflow todo marker: $needle"
		}
	}
	$workflowActionBody = [regex]::Match($mainTemplate, '(?s)void Managed_RequestWorkflowActionAdmin\(.*?\n\}\r?\n\r?\nvoid Managed_RequestWorkflowLogListAdmin').Value
	if ([string]::IsNullOrWhiteSpace($workflowActionBody)) {
		throw 'managed_main.c.tpl missing workflow action body for update-row guard check'
	}
	foreach ($needle in @(
		'bUpdated = sqlite3_changes(pDb) > 0 ? TRUE : FALSE',
		'content not found or deleted',
		'xvoTableSetBool(tblRet, "logSaved", 8',
		'xvoTableSetBool(tblRet, "notificationSaved", 17',
		'Managed_WorkflowSyncContentEffects(pDb, tblSpec, iId, iToStatus, bToDraft, iNow)'
	)) {
		if ($workflowActionBody -notmatch [regex]::Escape($needle)) {
			throw "managed_main.c.tpl workflow action missing update-row guard marker: $needle"
		}
	}
	foreach ($needle in @(
		'bool Managed_WorkflowNotify',
		'bool Managed_WorkflowAppendLog'
	)) {
		if ($mainTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_main.c.tpl missing workflow log helper marker: $needle"
		}
	}
	foreach ($needle in @(
		'maxListRows',
		'maxReasonLength',
		'requiredApprovals',
		'adminStats',
		'adminStatsUi',
		'actionResultUi',
		'scheduledPublishUi',
		'statsDiagnostics',
		'nodePlan',
		'transitionPlan',
		'workflowPlanUi',
		'workflow.stats',
		'multiApproval',
		'distinctApprovers',
		'approvalGate',
		'operatorTracking',
		'maxRequestBytes',
		'adminListFilters',
		'notification.readStatus',
		'listLimit',
		'reasonLimit',
		'requestLimit'
	)) {
		if ($workflowContracts -notmatch [regex]::Escape($needle)) {
			throw "content.workflow contracts.json missing list limit marker: $needle"
		}
	}
	Write-Output 'workflow due-run bounded UI wiring OK'
}

Invoke-Step 'smoke acceptance script syntax' {
	$smokeScript = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'tools/smoke_capability_acceptance.ps1')
	[scriptblock]::Create($smokeScript) | Out-Null
	$generatedSmokeScript = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'tools/smoke_generated_runtime.ps1')
	[scriptblock]::Create($generatedSmokeScript) | Out-Null
	$generatedLiveSmokeScript = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'tools/smoke_generated_runtime_live.ps1')
	[scriptblock]::Create($generatedLiveSmokeScript) | Out-Null
	$contentGenerationSmokeScript = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'tools/smoke_content_generation_live.ps1')
	[scriptblock]::Create($contentGenerationSmokeScript) | Out-Null
	$cmsWorkflowScript = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'tools/check_cms_capability_workflow.ps1')
	[scriptblock]::Create($cmsWorkflowScript) | Out-Null
	$adminCookieScript = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'tools/get_admin_cookie.ps1')
	[scriptblock]::Create($adminCookieScript) | Out-Null
	$contentGenerationSmokeFixture = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'tools/fixtures/content_smoke_model.json')
	foreach ($needle in @(
		'capability-pack',
		'acceptanceApiPath',
		'acceptanceViewPath',
		'ManifestPath',
		'RuntimeDir',
		'RuntimeDir not found',
		'ManagedPath',
		'ContractsPath',
		'ValidateManifestOnly',
		'Resolve-RuntimeSiblingPath',
		'Read-PackIdsFromRuntimeManifest',
		'Read-PackIdsFromRuntimeManaged',
		'Read-PackIdsFromRuntimeContracts',
		'Assert-SamePackIds',
		'loadPolicy must be enabled-packs-only',
		'abilityPacks',
		'capabilitySlots',
		'runtime manifest/managed enabled pack list',
		'runtime manifest/contracts enabled pack list',
		'ValidateManifestOnly requires ManifestPath',
		'duplicate packId',
		'abilityPacks item missing packId',
		'SkipContentCheck',
		'viewUrl',
		'Test-SmokeUrl',
		'Resolve-AcceptancePath',
		'Normalize-PackIdList',
		'contains empty packId',
		'contains invalid packId',
		'contains duplicate packId',
		'must be site-relative',
		'must not be an external URL',
		'must include {pluginXid}',
		"Test-SmokeUrl `$url `$TimeoutSec 'api'",
		"Test-SmokeUrl `$viewUrl `$TimeoutSec 'view'",
		'viewStatus',
		'Summary',
		'api2xx',
		'api3xx',
		'apiAuth',
		'viewChecked',
		'view2xx',
		'view3xx',
		'viewAuth',
		'ConvertFrom-Json',
		'contentOk',
		'viewContentOk',
		'ContentCheck',
		'response does not look like JSON',
		'response does not look like an xAdmin HTML page',
		'$status -eq 401 -or $status -eq 403',
		'Read-PackManifest',
		'Invoke-WebRequest'
	)) {
		if ($smokeScript -notmatch [regex]::Escape($needle)) {
			throw "smoke_capability_acceptance.ps1 missing manifest-driven marker: $needle"
		}
	}
	foreach ($needle in @(
		'RuntimeDir',
		'managed.json',
		'AdminBase',
		'CookieHeader',
		'capability.manifest.json',
		'contracts.json',
		'PluginXid is required or must be present in runtime/managed.json',
		'smoke_capability_acceptance.ps1',
		'$adminBase/view/plugin/$encodedXid',
		'$adminBase/api/plugin/$encodedXid/list?limit=1',
		'/api/plugin/$encodedXid/list?limit=1',
		'Test-GeneratedUrl',
		'ValidateManifestOnly',
		'CoreChecked',
		'Capability',
		'response does not look like JSON',
		'response does not look like an xAdmin HTML page',
		'$status -eq 401 -or $status -eq 403'
	)) {
		if ($generatedSmokeScript -notmatch [regex]::Escape($needle)) {
			throw "smoke_generated_runtime.ps1 missing generated-runtime marker: $needle"
		}
	}
	foreach ($needle in @(
		'StartServer',
		'StopStartedServer',
		'Start-Process',
		'-WindowStyle Hidden',
		'GenerateXid',
		'AdminBase',
		'CookieHeader',
		'Invoke-ContentGenerate',
		'/content/generate?xid=',
		'content generate response missing data.pluginXid',
		'RuntimeDir is required unless GenerateXid or PluginXid can resolve',
		'Wait-LiveServer',
		'Test-LiveServer',
		"baseUrl.TrimEnd('/') + '/'",
		'smoke_generated_runtime.ps1',
		'server is not reachable',
		'SkipContentCheck'
	)) {
		if ($generatedLiveSmokeScript -notmatch [regex]::Escape($needle)) {
			throw "smoke_generated_runtime_live.ps1 missing live generated-runtime marker: $needle"
		}
	}
	foreach ($needle in @(
		'fixtures/content_smoke_model.json',
		'AdminBase',
		'CookieHeader',
		'/content/save',
		'Invoke-ContentSave',
		'Start-Process',
		'-WindowStyle Hidden',
		'Wait-LiveServer',
		"baseUrl.TrimEnd('/') + '/'",
		'server is not reachable',
		'GenerateXid',
		'EnableGeneratedPlugin',
		'smoke_generated_runtime_live.ps1'
	)) {
		if ($contentGenerationSmokeScript -notmatch [regex]::Escape($needle)) {
			throw "smoke_content_generation_live.ps1 missing generation-live marker: $needle"
		}
	}
	foreach ($needle in @(
		'"xid": "qa.content_smoke"',
		'"generatedPluginXid": "cms.qa_content_smoke"',
		'"content.category"',
		'"content.import-export"'
	)) {
		if ($contentGenerationSmokeFixture -notmatch [regex]::Escape($needle)) {
			throw "content_smoke_model.json missing smoke fixture marker: $needle"
		}
	}
	foreach ($needle in @(
		'check_capability_packs.ps1',
		'check_content_system.ps1',
		'git diff --check',
		'smoke_content_generation_live.ps1',
		'RunLiveSmoke',
		'CookieHeader',
		'AdminUsername',
		'AdminPasswordHash',
		'get_admin_cookie.ps1',
		'AdminBase',
		'AdminLoginPath',
		'Use -AdminLoginPath when the site enables a custom admin entry',
		'CMS capability workflow checks OK'
	)) {
		if ($cmsWorkflowScript -notmatch [regex]::Escape($needle)) {
			throw "check_cms_capability_workflow.ps1 missing workflow marker: $needle"
		}
	}
	foreach ($needle in @(
		'XADMIN_SMOKE_USER',
		'XADMIN_SMOKE_PASSWORD',
		'XADMIN_SMOKE_PASSWORD_HASH',
		'Get-Sha256Hex',
		'_xywhsoft_',
		'AdminLoginPath',
		'/login',
		'Set-Cookie',
		'XSID=',
		'Write-Output'
	)) {
		if ($adminCookieScript -notmatch [regex]::Escape($needle)) {
			throw "get_admin_cookie.ps1 missing admin cookie marker: $needle"
		}
	}
	$packCheckScript = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'tools/check_capability_packs.ps1')
	foreach ($needle in @(
		'packId must be a lowercase English dot-separated id',
		'display title must be localized Chinese text',
		'acceptanceApiPath must be a low-cost read/list/stats/check route',
		'acceptanceApiPath must not use revision diff route',
		'acceptanceApiPath must not use redirect resolve route',
		'Convert-AcceptancePathToApiKey',
		'is missing from contracts.adminApis',
		'is missing from contracts.publicApis',
		'contracts API key must use dot-separated form',
		'contracts API key has invalid format',
		'Convert-AcceptancePathToRoutePath',
		'acceptanceApiPath route is not registered in managed_main.c.tpl'
	)) {
		if ($packCheckScript -notmatch [regex]::Escape($needle)) {
			throw "check_capability_packs.ps1 missing capability display boundary marker: $needle"
		}
	}
	$tmp = Join-Path $env:TEMP ('xadmin-smoke-manifest-' + [guid]::NewGuid().ToString('N'))
	New-Item -ItemType Directory -Path $tmp | Out-Null
	try {
		$manifestPath = Join-Path $tmp 'capability.manifest.json'
		$managedPath = Join-Path $tmp 'managed.json'
		$contractsPath = Join-Path $tmp 'contracts.json'
		Set-Content -Encoding UTF8 -Path $manifestPath -Value '{"loadPolicy":"enabled-packs-only","abilityPacks":[{"packId":"content.slug"},{"packId":"content.seo"}]}'
		Set-Content -Encoding UTF8 -Path $managedPath -Value '{"managed":true,"capabilitySlots":[{"key":"content.slug"},{"key":"content.seo"}]}'
		Set-Content -Encoding UTF8 -Path $contractsPath -Value '{"abilityPacks":[{"packId":"content.slug"},{"packId":"content.seo"}],"capabilities":[{"key":"content.slug"},{"key":"content.seo"}]}'
		$oldErrorActionPreference = $ErrorActionPreference
		$ErrorActionPreference = 'Continue'
		try {
			$okOutput = & powershell -ExecutionPolicy Bypass -File (Join-Path $Root 'tools/smoke_capability_acceptance.ps1') -Root $Root -PluginXid 'demo.content' -RuntimeDir $tmp -ValidateManifestOnly 2>&1
			if ($LASTEXITCODE -ne 0) {
				throw "manifest-only smoke positive check failed: $okOutput"
			}
			Set-Content -Encoding UTF8 -Path $managedPath -Value '{"managed":true,"capabilitySlots":[{"key":"content.redirect"}]}'
			$badOutput = & powershell -ExecutionPolicy Bypass -File (Join-Path $Root 'tools/smoke_capability_acceptance.ps1') -Root $Root -PluginXid 'demo.content' -ManifestPath $manifestPath -ValidateManifestOnly 2>&1
			if ($LASTEXITCODE -eq 0) {
				throw "manifest-only smoke negative check unexpectedly passed: $badOutput"
			}
		} finally {
			$ErrorActionPreference = $oldErrorActionPreference
		}
	} finally {
		Remove-Item -LiteralPath $tmp -Recurse -Force -ErrorAction SilentlyContinue
	}
	Write-Output 'smoke_capability_acceptance syntax OK'
}

Invoke-Step 'cms capability workflow wiring' {
	$workflowPath = Join-Path $Root '.github/workflows/cms-capability.yml'
	if (!(Test-Path $workflowPath)) {
		throw 'cms capability workflow is missing'
	}
	$workflow = Get-Content -Raw -Encoding UTF8 $workflowPath
	foreach ($needle in @(
		'tools\check_capability_packs.ps1',
		'tools\check_content_system.ps1',
		'git diff --check',
		'hosts/xadmin/capability-pack/**',
		'hosts/xadmin/data/content/templates/**',
		'hosts/xadmin/script/content/**',
		'hosts/xadmin/wwwroot/content/js/**'
	)) {
		if ($workflow -notmatch [regex]::Escape($needle)) {
			throw "cms capability workflow missing marker: $needle"
		}
	}
	Write-Output 'cms capability workflow wiring OK'
}

Write-Output 'content system checks OK'
