[CmdletBinding()]
param(
    [string] $VcpkgDir,
    [string] $VcpkgRepository,
    [string] $VcpkgRevision
)

Set-StrictMode -Version 2
$ErrorActionPreference = 'Stop'

$packages = (
    'cpprestsdk[core]',
    'fmt',
    'outcome',
    'wtl'
)

$triplets = ('x86-windows-static-md-noiso')
$nugetPackageId = 'foo_scrobble-deps'
$nugetPackageVersion = '1.0.0'

if (! $VcpkgRepository) {
    $VcpkgRepository = 'https://github.com/gix/vcpkg.git'
}

if (! $VcpkgRevision) {
    $VcpkgRevision = '56ec9296902d6b026f5617cbf7a99b896c3e3fbb'
}

# ------------------------------------------------------------------------------

$engDir      = $PSScriptRoot
$rootDir     = Split-Path -Parent $engDir
$packagesDir = Join-Path $rootDir 'src' | Join-Path -ChildPath 'packages'

function Step()
{
    param(
        [Parameter(Mandatory=$true, Position=0)]
        [ScriptBlock] $ScriptBlock,
        [string] $Status,
        [string] $Failure,
        [int] $N
    )

    $activity = 'Building dependencies'

    $verbose = $VerbosePreference -ne 'SilentlyContinue'
    $eapPrev = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'

    try {
        if ($Activity) {
            if ($verbose) {
                $msg = "[$($N+1)/6] ${activity}: ${Status}"
                Write-Output ''
                Write-Output "--- ${msg} $([string]::new('-', 80 - 5 - $msg.Length))"
                Write-Output ''
            } else {
                if ($Status) {
                    Write-Progress -Id 1 -Activity $activity -Status $Status -PercentComplete ($N*100/6)
                } else {
                    Write-Progress -Id 1 -Activity $activity
                }
            }
        }

        if ($verbose) {
            & $scriptBlock
        } else {
            $out = & $scriptBlock
        }

        if ($LASTEXITCODE -ne 0) {
            if (! $verbose) {
                Write-Output $out
            }

            if (! $Failure) { $Failure = "Command failed" }
            throw $Failure
        }
    } finally {
        $ErrorActionPreference = $eapPrev
    }
}

$Activity = 'Building dependencies'

if (! $VcpkgDir) {
    $VcpkgDir = Join-Path (Join-Path $rootDir 'build') 'deps'
    Write-Output "Using vcpkg in $VcpkgDir"
}

$vcpkg = Join-Path $VcpkgDir 'vcpkg.exe'

# Download nuget
$nuget = Join-Path (Join-Path $rootDir 'build') 'nuget.exe'
if (-not (Test-Path $nuget)) {
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    Write-Output 'Downloading nuget commandline client'
    New-Item -ItemType Directory -Force -Path (Join-Path $rootDir 'build')
    Invoke-WebRequest -Uri https://dist.nuget.org/win-x86-commandline/v5.8.0/nuget.exe -OutFile $nuget -UseBasicParsing
}

# Download and bootstrap vcpkg
if (-not (Test-Path $vcpkg)) {
    if (-not (Test-Path (Join-Path $VcpkgDir '.git'))) {
        $git = Get-Command git

        Step -N 0 -Status 'Downloading vcpkg' -Failure "Failed to git clone vcpkg repository" {
            &$git clone -b master $VcpkgRepository $VcpkgDir 2>&1
        }

        Step {
            &$git -C $VcpkgDir -c advice.detachedHead=false checkout $VcpkgRevision 2>&1
        }
    }

    Step -N 1 -Status 'Bootstrapping vcpkg' -Failure "vcpkg bootstrap failed" {
        &(Join-Path $VcpkgDir 'bootstrap-vcpkg.bat') -disableMetrics
    }
}

# Install custom triplets
foreach ($triplet in $triplets) {
    $srcFile = Join-Path $engDir "${triplet}.cmake"
    $dstFile = Join-Path $VcpkgDir (Join-Path 'triplets' "${triplet}.cmake")
    if (Test-Path $dstFile) {
        if (Compare-Object (Get-Content $srcFile) (Get-Content $dstFile)) {
            throw "Triplet file '$dstFile' already exists, but is different from '$srcFile'."
        }
    } else {
        Copy-Item -Path $srcFile -Destination $dstFile
    }
}

$qualifiedPackages = $triplets | foreach { $t = $_; $packages | foreach { "${_}:${t}" } }

Step -N 2 -Status 'Removing old package versions' -Failure 'vcpkg remove failed' {
    &$vcpkg remove --recurse $qualifiedPackages
}

Step -N 3 -Status 'Installing packages' -Failure 'vcpkg install failed' {
    &$vcpkg install $qualifiedPackages
}

Step -N 4 -Status 'Exporting packages' -Failure 'vcpkg export failed' {
    &$vcpkg export $qualifiedPackages --nuget --nuget-version=$nugetPackageVersion --nuget-id=$nugetPackageId
}

# Install the package to the solution packages directory
Step -N 5 -Status 'Installing Nuget package to solution' -Failure 'nuget install failed' {
    Write-Output "Installing Nuget package to $packagesDir"
    &$nuget install $nugetPackageId -Version $nugetPackageVersion -Source $VcpkgDir `
            -OutputDirectory $packagesDir -NonInteractive -Verbosity quiet
}

Write-Output 'Installation successful.'
