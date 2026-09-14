param(
	[string]$Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path,
	[string]$BaseUrl = 'http://127.0.0.1',
	[string]$AdminBase = '/admin',
	[string]$AdminLoginPath = '',
	[string]$CookieHeader = '',
	[string]$AdminUsername = '',
	[string]$AdminPassword = '',
	[string]$AdminPasswordHash = '',
	[int]$TimeoutSec = 10,
	[int]$StartupTimeoutSec = 20,
	[switch]$RunLiveSmoke,
	[switch]$StartServer,
	[switch]$StopStartedServer,
	[switch]$RememberLogin,
	[switch]$SkipContentCheck
)

$ErrorActionPreference = 'Stop'

function Invoke-WorkflowStep($name, $scriptBlock) {
	Write-Host "== $name =="
	& $scriptBlock
	Write-Host ""
}

function Invoke-ToolScript($scriptName, $arguments) {
	$scriptPath = Join-Path $PSScriptRoot $scriptName
	if (!(Test-Path $scriptPath)) {
		throw "required script not found: $scriptPath"
	}
	& powershell -ExecutionPolicy Bypass -File $scriptPath @arguments
	if ($LASTEXITCODE -ne 0) {
		throw "$scriptName failed with exit code $LASTEXITCODE"
	}
}

Push-Location $Root
try {
	Invoke-WorkflowStep 'capability pack contracts' {
		Invoke-ToolScript 'check_capability_packs.ps1' @()
	}

	Invoke-WorkflowStep 'content system static gate' {
		Invoke-ToolScript 'check_content_system.ps1' @()
	}

	Invoke-WorkflowStep 'git diff whitespace gate' {
		git diff --check
		if ($LASTEXITCODE -ne 0) {
			throw "git diff --check failed with exit code $LASTEXITCODE"
		}
	}

	if ($RunLiveSmoke) {
		if ([string]::IsNullOrWhiteSpace($CookieHeader) -and !$StartServer) {
			$loginArgs = @(
				'-BaseUrl', $BaseUrl,
				'-AdminBase', $AdminBase,
				'-TimeoutSec', $TimeoutSec
			)
			if (![string]::IsNullOrWhiteSpace($AdminLoginPath)) { $loginArgs += @('-AdminLoginPath', $AdminLoginPath) }
			if (![string]::IsNullOrWhiteSpace($AdminUsername)) { $loginArgs += @('-Username', $AdminUsername) }
			if (![string]::IsNullOrWhiteSpace($AdminPassword)) { $loginArgs += @('-Password', $AdminPassword) }
			if (![string]::IsNullOrWhiteSpace($AdminPasswordHash)) { $loginArgs += @('-PasswordHash', $AdminPasswordHash) }
			if ($RememberLogin) { $loginArgs += '-Remember' }
			try {
				$CookieHeader = (& powershell -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'get_admin_cookie.ps1') @loginArgs | Select-Object -Last 1)
			} catch {
				throw 'RunLiveSmoke requires CookieHeader, or valid AdminUsername/AdminPassword credentials for get_admin_cookie.ps1. ' + $_.Exception.Message
			}
			if ([string]::IsNullOrWhiteSpace($CookieHeader)) {
				throw 'get_admin_cookie.ps1 did not return a CookieHeader'
			}
		}
		Invoke-WorkflowStep 'generated runtime live smoke' {
			$args = @(
				'-Root', $Root,
				'-BaseUrl', $BaseUrl,
				'-AdminBase', $AdminBase,
				'-TimeoutSec', $TimeoutSec,
				'-StartupTimeoutSec', $StartupTimeoutSec
			)
			if (![string]::IsNullOrWhiteSpace($CookieHeader)) { $args += @('-CookieHeader', $CookieHeader) }
			if (![string]::IsNullOrWhiteSpace($AdminLoginPath)) { $args += @('-AdminLoginPath', $AdminLoginPath) }
			if (![string]::IsNullOrWhiteSpace($AdminUsername)) { $args += @('-AdminUsername', $AdminUsername) }
			if (![string]::IsNullOrWhiteSpace($AdminPassword)) { $args += @('-AdminPassword', $AdminPassword) }
			if (![string]::IsNullOrWhiteSpace($AdminPasswordHash)) { $args += @('-AdminPasswordHash', $AdminPasswordHash) }
			if ($RememberLogin) { $args += '-RememberLogin' }
			if ($StartServer) { $args += '-StartServer' }
			if ($StopStartedServer) { $args += '-StopStartedServer' }
			if ($SkipContentCheck) { $args += '-SkipContentCheck' }
			Invoke-ToolScript 'smoke_content_generation_live.ps1' $args
		}
	} else {
		Write-Host '== generated runtime live smoke =='
		Write-Host 'SKIP: pass -RunLiveSmoke with -CookieHeader "XSID=..." or -AdminUsername/-AdminPassword to run the protected admin HTTP path. Use -AdminLoginPath when the site enables a custom admin entry.'
		Write-Host ''
	}

	Write-Host 'CMS capability workflow checks OK'
} finally {
	Pop-Location
}
