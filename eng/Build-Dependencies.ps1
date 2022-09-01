<#
.SYNOPSIS
  Builds a NuGet package of all required dependencies.

.DESCRIPTION
  The Build-Deps cmdlet invokes vcpkg to build all required dependencies and
  exports them as NuGet package consumable by Visual Studio's MSBuild tooling
  for C++.

.PARAMETER Version
  Overrides the version of the dependency package.

.PARAMETER VcpkgDir
  Overrides the path to the vcpkg directory.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$false)]
    [string] $Version = '2.0.0',

    [Parameter(Mandatory=$false)]
    [string] $VcpkgDir,

    [switch] $Clean = $true,
    [switch] $Build = $true,
    [switch] $Install,

    [Parameter(DontShow)]
    [string] $VcpkgRepository,
    [Parameter(DontShow)]
    [string] $VcpkgRevision,
    [Parameter(DontShow)]
    [switch] $UseHeadRevision,
    [Parameter(DontShow)]
    [switch] $NoBinaryCache
)

Set-StrictMode -Version 2
$ErrorActionPreference = 'Stop'

$packages = (
    'cpprestsdk[core]',
    'fmt',
    'outcome',
    'wtl'
)

$triplets = ('x86-windows-static-md-noiso', 'x64-windows-static-md-noiso')

[string]$NugetPackageId = 'foo_scrobble-deps'
[string]$NugetPackageVersion = $Version

[string]$VcpkgBranch = 'foo_scrobble'

if (! $VcpkgRepository) {
    $VcpkgRepository = 'https://github.com/gix/vcpkg.git'
}
if (! $VcpkgRevision) {
    $VcpkgRevision = 'b4c94da052e881f8cb5044d8e5b88f5ff1f0519f'
}

# ------------------------------------------------------------------------------

[string]$RepositoryRoot = Resolve-Path (Join-Path $PSScriptRoot '..\')
[string]$RepositoryEngineeringDir = Join-Path $RepositoryRoot 'eng'
[string]$ArtifactsDir = Join-Path $RepositoryRoot 'artifacts'
[string]$ArtifactsTmpDir = Join-Path $ArtifactsDir 'tmp'
[string]$PackagesDir = Join-Path $RepositoryRoot 'src\packages'

if ($UseHeadRevision) {
    $VcpkgRevision = $VcpkgBranch
}

if (! $VcpkgDir) {
    $VcpkgDir = Join-Path $ArtifactsDir 'vcpkg'
    Write-Output "Using vcpkg in $VcpkgDir"
}

function Create-Directory([string[]] $Path)
{
    New-Item -Path $Path -Force -ItemType 'Directory' > $null
}

function Verify-Signature([string[]] $Path)
{
    $signature = Get-AuthenticodeSignature $Path
    if ($signature.Status -ne 'Valid') {
        Write-Error "Failed to verify '$Path': $($signature.StatusMessage)"
    }
}

function Step()
{
    param(
        [Parameter(Mandatory=$true, Position=0)]
        [ScriptBlock] $ScriptBlock,
        [string] $Status,
        [string] $Failure,
        [int] $N,
        [int] $Total = 7
    )

    $Activity = 'Building dependencies'

    $verbose = $VerbosePreference -ne 'SilentlyContinue'
    $eapPrev = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'

    try {
        if ($Activity) {
            if ($verbose) {
                $msg = "[$($N+1)/$Total] ${activity}: ${Status}"
                Write-Host ''
                Write-Host -Foreground Blue "--- ${msg} $([string]::new('-', 80 - 5 - $msg.Length))"
                Write-Host ''
            } else {
                if ($Status) {
                    Write-Progress -Id 1 -Activity $activity -Status $Status -PercentComplete ($N*100/$Total)
                } else {
                    Write-Progress -Id 1 -Activity $activity
                }
            }
        }

        $global:LASTEXITCODE = 0
        if ($verbose) {
            & $ScriptBlock
        } else {
            $out = & $ScriptBlock
        }

        if ($global:LASTEXITCODE -ne 0) {
            if (! $verbose) {
                Write-Host $out
            }

            if (! $Failure) { $Failure = "Command failed" }
            throw $Failure
        }
    } finally {
        $ErrorActionPreference = $eapPrev
    }
}

function Bootstrap-Vcpkg()
{
    $vcpkg = Join-Path $VcpkgDir 'vcpkg.exe'
    $git = Get-Command git

    if (-not (Test-Path (Join-Path $VcpkgDir '.git'))) {
        Step -N 1 -Status 'Downloading vcpkg' -Failure "Failed to git clone vcpkg repository" {
            &$git clone -b $VcpkgBranch -n $VcpkgRepository $VcpkgDir 2>&1 | Out-Host
        }
    }

    Step -N 2 -Status 'Checking out revision' -Failure "Failed to check out revision '$VcpkgRevision'" {
        &$git -C $VcpkgDir fetch $VcpkgRepository $VcpkgRevision 2>&1 | Out-Host
        &$git -C $VcpkgDir checkout $VcpkgRevision 2>&1 | Out-Host
    }

    Step -N 3 -Status 'Bootstrapping vcpkg' -Failure "vcpkg bootstrap failed" {
        if (-not (Test-Path $vcpkg)) {
            &(Join-Path $VcpkgDir 'bootstrap-vcpkg.bat') -disableMetrics | Out-Host
        }
    }

    return $vcpkg
}

if ($Clean) {
    Step -N 0 -Status 'Cleaning' -Failure "Failed to clean vcpkg dir" {
        foreach ($name in @('buildtrees', 'downloads', 'installed', 'packages', 'vcpkg.exe')) {
            Write-Host "Cleaning $name..."
            $path = Join-Path $VcpkgDir $name
            if (Test-Path $path) {
                Remove-Item -Recurse -Force $path
            }
        }
    }

    Write-Host 'Cleaning done'
}

$vcpkg = Bootstrap-Vcpkg

if ($Build) {
    foreach ($triplet in $triplets) {
        Copy-Item -Force -Path (Join-Path $RepositoryEngineeringDir 'cmake' "${triplet}.cmake") `
                  -Destination (Join-Path $VcpkgDir 'triplets')
    }

    $installPackages = $triplets | foreach { $t = $_; $packages | foreach { "${_}:${t}" } }
    $exportPackages = $installPackages | foreach { $_ -replace '\[[^\]]+?\]','' }

    $featureFlags = @()
    if ($NoBinaryCache) {
        $featureFlags += '--feature-flags=-binarycaching'
    }

    Step -N 4 -Status 'Installing packages' -Failure 'vcpkg install failed' {
        &$vcpkg install $installPackages $featureFlags
    }

    Step -N 5 -Status 'Exporting packages' -Failure 'vcpkg export failed' {
        &$vcpkg export $exportPackages --nuget --nuget-version=$NugetPackageVersion --nuget-id=$NugetPackageId
    }
}

if ($Install) {
    # Install the package to the solution packages directory
    $nuget = Get-ChildItem (Join-Path $VcpkgDir 'downloads\tools\nuget\*\nuget.exe') -ErrorAction SilentlyContinue | sort -Descending | select -First 1
    if (! $nuget) {
        $nuget = Get-Command nuget -ErrorAction SilentlyContinue
    }

    if (! $nuget) {
        Write-Host 'Downloading nuget commandline client'
        $nuget = Join-Path $ArtifactsTmpDir 'nuget.exe'
        Create-Directory $ArtifactsTmpDir
        Invoke-WebRequest -Uri https://dist.nuget.org/win-x86-commandline/v6.0.0/nuget.exe -OutFile $nuget -UseBasicParsing
        Verify-Signature $nuget
    }

    Step -N 6 -Status 'Installing Nuget package to solution' -Failure 'nuget install failed' {
        Write-Host "Installing Nuget package to $PackagesDir"
        &$nuget install $nugetPackageId -Version $NugetPackageVersion -Source $VcpkgDir `
                -OutputDirectory $PackagesDir -NonInteractive -Verbosity quiet
    }
}

Write-Output 'Installation successful.'
