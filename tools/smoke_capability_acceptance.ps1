param(
	[string]$BaseUrl = 'http://127.0.0.1:8080',
	[string]$PluginXid,
	[string[]]$PackId = @(),
	[int]$TimeoutSec = 10
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($PluginXid)) {
	throw 'PluginXid is required'
}

$acceptance = @{
	'content.category' = '/api/plugin/{pluginXid}/category/list'
	'content.seo' = '/api/plugin/{pluginXid}/seo/meta?id=1'
	'content.slug' = '/api/plugin/{pluginXid}/slug/resolve?slug=sample'
	'content.redirect' = '/api/plugin/{pluginXid}/redirect/resolve?path=/old'
	'content.media' = '/api/plugin/{pluginXid}/media/list'
	'content.revision' = '/admin/api/plugin/{pluginXid}/revision/list'
	'content.workflow' = '/admin/api/plugin/{pluginXid}/workflow/log/list'
	'content.search' = '/api/plugin/{pluginXid}/search?q=test'
	'content.sitemap' = '/api/plugin/{pluginXid}/sitemap.xml'
	'content.related' = '/api/plugin/{pluginXid}/related/list?contentId=1'
	'content.form' = '/admin/api/plugin/{pluginXid}/form/list'
	'content.access' = '/api/plugin/{pluginXid}/access/check?id=1'
	'content.audit-log' = '/admin/api/plugin/{pluginXid}/audit-log/list'
	'content.import-export' = '/admin/api/plugin/{pluginXid}/import-export/import/jobs'
	'content.comment' = '/api/plugin/{pluginXid}/comment/list?contentId=1'
	'content.tag' = '/api/plugin/{pluginXid}/tag/list'
	'content.topic' = '/api/plugin/{pluginXid}/topic/list'
	'content.sensitive' = '/admin/api/plugin/{pluginXid}/sensitive/word/list'
	'content.static' = '/admin/api/plugin/{pluginXid}/static/task/list'
	'content.like' = '/api/plugin/{pluginXid}/like/count?contentId=1'
	'content.view-stat' = '/admin/api/plugin/{pluginXid}/view/counter/list'
}

if ($PackId.Count -eq 0) {
	$PackId = $acceptance.Keys | Sort-Object
}

$base = $BaseUrl.TrimEnd('/')
$results = New-Object System.Collections.Generic.List[object]
$errors = New-Object System.Collections.Generic.List[string]

foreach ($pack in $PackId) {
	if (!$acceptance.ContainsKey($pack)) {
		$errors.Add("$pack has no acceptance path") | Out-Null
		continue
	}
	$path = $acceptance[$pack].Replace('{pluginXid}', [uri]::EscapeDataString($PluginXid))
	$url = $base + $path
	$status = 0
	$ok = $false
	$errorText = ''
	try {
		$response = Invoke-WebRequest -Uri $url -Method Get -TimeoutSec $TimeoutSec -UseBasicParsing
		$status = [int]$response.StatusCode
		$ok = $status -ge 200 -and $status -lt 500
	} catch {
		$status = if ($_.Exception.Response) { [int]$_.Exception.Response.StatusCode } else { 0 }
		$errorText = $_.Exception.Message
		$ok = $status -ge 200 -and $status -lt 500
	}
	if (!$ok) {
		$errors.Add("$pack failed: status=$status url=$url $errorText") | Out-Null
	}
	$results.Add([pscustomobject]@{
		packId = $pack
		url = $url
		status = $status
		ok = $ok
		error = $errorText
	}) | Out-Null
}

[pscustomobject]@{
	PluginXid = $PluginXid
	BaseUrl = $base
	Checked = $results.Count
	ErrorCount = $errors.Count
	Results = @($results)
	Errors = @($errors)
} | ConvertTo-Json -Depth 6

if ($errors.Count -gt 0) {
	exit 1
}
