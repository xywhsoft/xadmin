param(
	[string]$BaseUrl = 'http://127.0.0.1',
	[string]$AdminBase = '/admin',
	[string]$AdminLoginPath = '',
	[string]$Username = '',
	[string]$Password = '',
	[string]$PasswordHash = '',
	[int]$TimeoutSec = 10,
	[switch]$Remember
)

$ErrorActionPreference = 'Stop'

function Get-Sha256Hex([string]$Text) {
	$sha = [System.Security.Cryptography.SHA256]::Create()
	try {
		$bytes = [System.Text.Encoding]::UTF8.GetBytes($Text)
		$hash = $sha.ComputeHash($bytes)
		return -join ($hash | ForEach-Object { $_.ToString('x2') })
	} finally {
		$sha.Dispose()
	}
}

if ([string]::IsNullOrWhiteSpace($Username)) {
	$Username = [string]$env:XADMIN_SMOKE_USER
}
if ([string]::IsNullOrWhiteSpace($Password)) {
	$Password = [string]$env:XADMIN_SMOKE_PASSWORD
}
if ([string]::IsNullOrWhiteSpace($PasswordHash)) {
	$PasswordHash = [string]$env:XADMIN_SMOKE_PASSWORD_HASH
}
if ([string]::IsNullOrWhiteSpace($Username)) {
	throw 'Username is required. Pass -Username or set XADMIN_SMOKE_USER.'
}
if ([string]::IsNullOrWhiteSpace($PasswordHash)) {
	if ([string]::IsNullOrWhiteSpace($Password)) {
		throw 'Password or PasswordHash is required. Pass -Password, -PasswordHash, XADMIN_SMOKE_PASSWORD or XADMIN_SMOKE_PASSWORD_HASH.'
	}
	$PasswordHash = Get-Sha256Hex ($Username + '_xywhsoft_' + $Password)
}

if ([string]::IsNullOrWhiteSpace($AdminLoginPath)) {
	$adminBasePath = '/' + $AdminBase.Trim('/')
	$AdminLoginPath = $adminBasePath + '/login'
} else {
	$AdminLoginPath = '/' + $AdminLoginPath.Trim('/')
}
$url = $BaseUrl.TrimEnd('/') + $AdminLoginPath
$body = @{
	username = $Username
	password = $PasswordHash
} 
if ($Remember) {
	$body.remember = 'on'
}

try {
	$response = Invoke-WebRequest -Uri $url -Method Post -TimeoutSec $TimeoutSec -UseBasicParsing -ContentType 'application/json' -Body ($body | ConvertTo-Json -Compress)
	$json = [string]$response.Content | ConvertFrom-Json
	if ($true -ne [bool]$json.result) {
		throw "admin login failed: $($json.message)"
	}
	$setCookie = @($response.Headers['Set-Cookie'])
	$cookieLine = $setCookie | Where-Object { $_ -match 'XSID=([^;]+)' } | Select-Object -First 1
	if ([string]::IsNullOrWhiteSpace($cookieLine)) {
		throw 'admin login response did not include XSID Set-Cookie header'
	}
	if ($cookieLine -notmatch 'XSID=([^;]+)') {
		throw 'admin login Set-Cookie header is invalid'
	}
	Write-Output ('XSID=' + $Matches[1])
} catch {
	throw "admin login request failed: $url $($_.Exception.Message)"
}
