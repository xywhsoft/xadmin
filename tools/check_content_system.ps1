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

Invoke-Step 'managed template javascript' {
	$script = @'
const fs = require("fs");
const vm = require("vm");
for (const file of [
  "hosts/xadmin/data/content/templates/managed_editor.html.tpl",
  "hosts/xadmin/data/content/templates/managed_ability.html.tpl",
  "hosts/xadmin/data/content/templates/managed_category.html.tpl",
  "hosts/xadmin/data/content/templates/managed_dashboard.html.tpl",
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

Invoke-Step 'capability manifest generation wiring' {
	$generator = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/script/content/content_generator.h')
	$generation = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/script/content/content_generation.h')
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
	Write-Output 'capability manifest wiring OK'
}

Invoke-Step 'dynamic route risk warning wiring' {
	$route = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/script/module/define.h')
	$trace = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/script/route_http/trace.h')
	$mainTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_main.c.tpl')
	$abilityTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_ability.html.tpl')
	foreach ($needle in @(
		'DynamicRoute_GetLastWarningHTTP',
		'DynamicRoute_RecordPatternRisk',
		'static asset overlap risk'
	)) {
		if ($route -notmatch [regex]::Escape($needle)) {
			throw "define.h missing dynamic route warning marker: $needle"
		}
	}
	foreach ($needle in @(
		'dynamicLastWarning',
		'G_DynamicRouteTableHTTP.sLastWarning'
	)) {
		if ($trace -notmatch [regex]::Escape($needle)) {
			throw "trace.h missing dynamic route warning marker: $needle"
		}
	}
	foreach ($needle in @(
		'Managed_SlugBuildRiskWarning',
		'Managed_RedirectBuildRiskWarning',
		'Managed_StaticRuleBuildRiskWarning',
		'static pathPattern covers site root',
		'static pathPattern overlaps admin/API prefix',
		'static pathPattern looks like a static resource path',
		'"warning"'
	)) {
		if ($mainTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_main.c.tpl missing route risk warning marker: $needle"
		}
	}
	foreach ($needle in @(
		'ret.warning',
		'ret.data && ret.data.warning',
		'redirectImportResult'
	)) {
		if ($abilityTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_ability.html.tpl missing route risk warning UI marker: $needle"
		}
	}
	Write-Output 'dynamic route risk warning wiring OK'
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
		'/pack/list?pack=content.comment&status=0',
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
		'xvoTableSetValue(tblRet, "tree", 4',
		'xvoTableSetValue(tblNode, "children", 8',
		'parentId',
		'parent comment not found',
		'SELECT id FROM comment_item WHERE id=? AND content_id=? AND status=1 AND delete_time=0 LIMIT 1'
	)) {
		if ($mainTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_main.c.tpl missing comment tree marker: $needle"
		}
	}
	Write-Output 'comment tree public response wiring OK'
}

Invoke-Step 'revision visual diff wiring' {
	$abilityTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_ability.html.tpl')
	$editorTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_editor.html.tpl')
	foreach ($needle in @(
		'renderRevisionChangeTable',
		'renderRevisionValue',
		'revision-line-diff',
		'changed fields',
		'<th style="width:160px">Field</th><th>Before</th><th>After</th>'
	)) {
		if ($abilityTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_ability.html.tpl missing revision visual diff marker: $needle"
		}
	}
	foreach ($needle in @(
		'renderRevisionChangeDialog',
		'renderRevisionValue',
		'revision-line-diff',
		'changed fields',
		'<th style="width:160px">Field</th><th>Before</th><th>After</th>'
	)) {
		if ($editorTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_editor.html.tpl missing revision visual diff marker: $needle"
		}
	}
	Write-Output 'revision visual diff wiring OK'
}

Invoke-Step 'media dimension UI wiring' {
	$abilityTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_ability.html.tpl')
	$editorTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_editor.html.tpl')
	foreach ($needle in @(
		'btnMediaDetect_',
		'naturalWidth',
		'naturalHeight',
		'field:''width'',title:''W''',
		'field:''height'',title:''H'''
	)) {
		if ($abilityTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_ability.html.tpl missing media dimension UI marker: $needle"
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
		'parentId: Number(select && select.value || 0)'
	)) {
		if ($categoryTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_category.html.tpl missing category drag sort marker: $needle"
		}
	}
	foreach ($needle in @(
		'xvoTableExists(tblItem, "parentId", 8)',
		'Managed_CategoryRewriteDescendantPaths(pDb, iId',
		'parent_id=?, path=?, level=?, sort=?, update_time=?',
		'parent category cannot be descendant'
	)) {
		if ($mainTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_main.c.tpl missing category parent move marker: $needle"
		}
	}
	Write-Output 'category drag sort wiring OK'
}

Invoke-Step 'form schema designer wiring' {
	$abilityTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_ability.html.tpl')
	$mainTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_main.c.tpl')
	foreach ($needle in @(
		'appendFormSchemaField',
		'removeFormSchemaField',
		'moveFormSchemaField',
		'btnFormDesignerAdd',
		'btnFormDesignerRemove',
		'btnFormDesignerUp',
		'btnFormDesignerDown',
		'btnFormSchemaFormat',
		'btnFormSchemaValidate',
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
		'Managed_ResolveFieldList(tblField)',
		'option is invalid'
	)) {
		if ($mainTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_main.c.tpl missing form option validation marker: $needle"
		}
	}
	Write-Output 'form schema designer wiring OK'
}

Invoke-Step 'access password hash wiring' {
	$mainTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_main.c.tpl')
	$abilityTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_ability.html.tpl')
	foreach ($needle in @(
		'Managed_AccessPasswordEncodeLegacyXrt64',
		'xsha256:',
		'ServerHashPassword',
		'xrtMakeXIDS',
		'payRequired',
		'No order subsystem is wired yet',
		'SELECT access_mode,required_read_level,password_hash,member_group_ids,price'
	)) {
		if ($mainTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_main.c.tpl missing access password hash marker: $needle"
		}
	}
	if ($abilityTemplate -notmatch [regex]::Escape('public/login/level/group/password/paid/private')) {
		throw 'managed_ability.html.tpl missing access paid mode marker'
	}
	Write-Output 'access password hash wiring OK'
}

Invoke-Step 'audit request ip wiring' {
	$mainTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_main.c.tpl')
	foreach ($needle in @(
		'Managed_AuditRequestIp',
		'xsReqRemote(objReq)',
		'Managed_AuditLogCore',
		'Managed_AuditLogWithRequest',
		'content.create',
		'access_rule.save',
		'Managed_AuditLogWithRequest(pDb, "media"',
		'Managed_AuditLogWithRequest(pDb, "form"',
		'Managed_AuditLogWithRequest(pDb, "import_job"'
	)) {
		if ($mainTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_main.c.tpl missing audit request ip marker: $needle"
		}
	}
	if ($mainTemplate -match 'Managed_AuditLog\(pDb,[^\r\n]*objSession\);') {
		throw 'managed_main.c.tpl still has request audit calls without request ip'
	}
	Write-Output 'audit request ip wiring OK'
}

Invoke-Step 'sitemap cache metadata wiring' {
	$mainTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_main.c.tpl')
	foreach ($needle in @(
		'sitemap/cache.json',
		'write-through',
		'Managed_SitemapEntryCount(pDb)',
		'cacheEntryCount',
		'cachePolicy',
		'Managed_SitemapCacheTtlSeconds',
		'cacheTtlSeconds',
		'cacheExpired',
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
		'Sitemap cache metadata',
		'Cache entry count',
		'Cache TTL seconds',
		'Cache expired'
	)) {
		if ($abilityTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_ability.html.tpl missing sitemap stats UI marker: $needle"
		}
	}
	Write-Output 'sitemap cache metadata wiring OK'
}

Invoke-Step 'static access guard wiring' {
	$mainTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_main.c.tpl')
	foreach ($needle in @(
		'restricted content cannot generate public static output',
		'Managed_StaticCreateTask(pDb, iRuleId, "content", iTargetId',
		'!Managed_AccessCheckRule(pDb, iTargetId, NULL, NULL, FALSE, NULL)',
		'!Managed_AccessCheckRule(pDb, iContentId, NULL, NULL, FALSE, NULL)'
	)) {
		if ($mainTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_main.c.tpl missing static access guard marker: $needle"
		}
	}
	Write-Output 'static access guard wiring OK'
}

Invoke-Step 'import export paging UI wiring' {
	$abilityTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_ability.html.tpl')
	foreach ($needle in @(
		'importExportLastExport',
		'runImportExportExport',
		'btnExportJsonNext',
		'importExportResult',
		'nextOffset',
		'runImportExportChunked',
		'importExportChunkSize',
		'btnImportChunkCommit'
	)) {
		if ($abilityTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_ability.html.tpl missing import/export paging UI marker: $needle"
		}
	}
	Write-Output 'import/export paging UI wiring OK'
}

Invoke-Step 'related rebuild bounded UI wiring' {
	$abilityTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_ability.html.tpl')
	$mainTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_main.c.tpl')
	foreach ($needle in @(
		'relatedRebuildResult',
		'Limit is clamped to 1-20',
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
		'Managed_RelatedSyncPeers'
	)) {
		if ($mainTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_main.c.tpl missing related de-dup marker: $needle"
		}
	}
	Write-Output 'related rebuild bounded UI wiring OK'
}

Invoke-Step 'search probe UI wiring' {
	$abilityTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_ability.html.tpl')
	$mainTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_main.c.tpl')
	foreach ($needle in @(
		'runSearchProbe',
		'searchProbeQuery',
		'searchProbeResult',
		'btnSearchProbe',
		'search query is required',
		'if(limit > 20) limit = 20'
	)) {
		if ($abilityTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_ability.html.tpl missing search probe UI marker: $needle"
		}
	}
	foreach ($needle in @(
		'Managed_BuildSearchLikePattern',
		"(*p == ' ') || (*p == '\t')",
		'Managed_SearchBuildSnippet',
		'Managed_TextFindIgnoreCase',
		'xrtCopyStr((str)(sText + iStart), 240)'
	)) {
		if ($mainTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_main.c.tpl missing search query token marker: $needle"
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
	foreach ($needle in @(
		'Managed_RequestLikeStatsAdmin',
		'/like/stats',
		'SELECT COUNT(*),COALESCE(SUM(like_count),0)',
		'Managed_RequestViewStatsAdmin',
		'/view/stats',
		'SELECT COUNT(*),COALESCE(SUM(view_count),0)',
		'Managed_RequestDashboardView',
		'generated/dashboard.html',
		'/admin/view/plugin/{{PLUGIN_XID}}/dashboard'
	)) {
		if ($mainTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_main.c.tpl missing metric stats marker: $needle"
		}
	}
	foreach ($needle in @(
		'showMetricStats',
		'btnLikeStats',
		'btnViewStats',
		'/like/stats',
		'/view/stats'
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
		'dailyTrendRows'
	)) {
		if ($dashboardTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_dashboard.html.tpl missing metric dashboard marker: $needle"
		}
	}
	Write-Output 'metric stats wiring OK'
}

Invoke-Step 'workflow due-run bounded UI wiring' {
	$abilityTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_ability.html.tpl')
	$mainTemplate = Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'hosts/xadmin/data/content/templates/managed_main.c.tpl')
	$utf8 = [System.Text.Encoding]::UTF8
	$todoTitle = $utf8.GetString([byte[]]@(0xe5,0xae,0xa1,0xe6,0xa0,0xb8,0xe5,0xbe,0x85,0xe5,0x8a,0x9e))
	foreach ($needle in @(
		'workflowScheduledLimit',
		'workflowScheduledResult',
		'Due-run limit is clamped to 1-200',
		'if(limit > 200) limit = 200',
		'min="1" max="200"',
		'/workflow/todo/list',
		$todoTitle,
		'lastAction',
		'lastReason'
	)) {
		if ($abilityTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_ability.html.tpl missing workflow due-run bounded UI marker: $needle"
		}
	}
	foreach ($needle in @(
		'Managed_RequestWorkflowTodoListAdmin',
		'/workflow/todo/list',
		'content_workflow_log l ON l.id=',
		'c.delete_time=0 AND (c.is_draft=1 OR c.status=0)',
		'LIMIT 200'
	)) {
		if ($mainTemplate -notmatch [regex]::Escape($needle)) {
			throw "managed_main.c.tpl missing workflow todo marker: $needle"
		}
	}
	Write-Output 'workflow due-run bounded UI wiring OK'
}

Invoke-Step 'smoke acceptance script syntax' {
	[scriptblock]::Create((Get-Content -Raw -Encoding UTF8 (Join-Path $Root 'tools/smoke_capability_acceptance.ps1'))) | Out-Null
	Write-Output 'smoke_capability_acceptance syntax OK'
}

Write-Output 'content system checks OK'
