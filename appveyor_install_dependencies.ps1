#
# Script to install dependencies
#

# Environment
Set-Variable COMPILER_VERSION -option Constant -value ( [string] $Env:COMPILER_VERSION )
Set-Variable COMPILER_TOOLSET -option Constant -value ( [string] $Env:COMPILER_TOOLSET )
Set-Variable ROOT_DIR -option Constant -value ( [string] ( ( Get-Item -Path ".\" -Verbose ).FullName ) )
Set-Variable CONFIGURATION -option Constant -value ( [string] $Env:CONFIGURATION )

# Dependency configuration
Set-Variable SPDLOG_VERSION -option Constant -value ( [string] "1.x" )
Set-Variable ANGELSCRIPT_VERSION -option Constant -value ( [string] "2.32.0" )

<#
	.SYNOPSIS
	Generates, builds and installs a CMake project
	
	.DESCRIPTION
	Creates the directories build and install, generates the project files from the location $src_path
	
	Expects the current directory to be where the build and install directories will be created
	
	.PARAMETER generate_args
	Arguments to append to the build generation step. Can be a string or an array of strings
	
	.PARAMETER src_path
	Location of the root CMakeLists.txt. Defaults to a directory "src" in the current directory
#>
function CMake_GenerateBuildAndInstall( $generate_args, [string] $src_path = "src" )
{
	[string] $currentDir = ( Get-Item -Path ".\" -Verbose ).FullName
	[string] $full_src_path = Join-Path $currentDir $src_path

	# Directory name is suitable for project name
	[string] $projectName = Split-Path -Path $currentDir -Leaf

	New-Item build -type directory
	New-Item install -type directory

	cd build

	Write-Host "[Dependency] [$projectName] Generating" -foregroundcolor green
	& "$ROOT_DIR\cmake_appveyor\bin\cmake.exe" -G"$COMPILER_VERSION" -T"$COMPILER_TOOLSET" -DCMAKE_INSTALL_PREFIX="$currentDir/install" $generate_args $full_src_path

	Write-Host "[Dependency] [$projectName] Building" -foregroundcolor green
	& "$ROOT_DIR\cmake_appveyor\bin\cmake.exe" --build . --clean-first --config $CONFIGURATION

	Write-Host "[Dependency] [$projectName] Installing" -foregroundcolor green
	& "$ROOT_DIR\cmake_appveyor\bin\cmake.exe" --build . --target install --config $CONFIGURATION

	Write-Host "[Dependency] [$projectName] Done" -foregroundcolor green

	cd ..
}

Write-Host "=== (3/5) Downloading and installing dependencies ===" -foregroundcolor green

# Install only if it wasn't cached
if( !( Test-Path dependencies -pathType container ) )
{
	New-Item dependencies -type directory
	cd dependencies

	[string] $dependencies_dir = ( Get-Item -Path ".\" -Verbose ).FullName

	# Use a common directory structure:
	# dependencies/dependency_name/src
	# dependencies/dependency_name/build
	# dependencies/dependency_name/install

	# SpdLog BEGIN
	New-Item spdlog -type directory
	cd spdlog

	Write-Host "[Dependency] Building SpdLog" -foregroundcolor green

	# Acquire files
	wget "https://github.com/gabime/spdlog/archive/v$SPDLOG_VERSION.zip" -OutFile spdlog.zip
	cmd /c 7z x spdlog.zip -o"." -y
	Rename-Item "spdlog-$SPDLOG_VERSION" src

	# Override the compiler settings to use a static runtime, so linking with HLE doesn't cause problems
	CMake_GenerateBuildAndInstall( @( "-DBUILD_SHARED_LIBS=OFF", "-DCMAKE_USER_MAKE_RULES_OVERRIDE='$ROOT_DIR/cmake/c_flags_overrides.cmake'", "-DCMAKE_USER_MAKE_RULES_OVERRIDE_CXX='$ROOT_DIR/cmake/cxx_flags_overrides.cmake'" ) )

	cd install

	# We don't need these files, and it reduces cache size
	Remove-Item bin/*
	Remove-Item share/* -recurse

	cd $dependencies_dir
	# SpdLog END

	# Restore to old path
	cd $ROOT_DIR

	# Angelscript BEGIN
	New-Item angelscript -type directory
	cd angelscript

	Write-Host "[Dependency] Building Angelscript" -foregroundcolor green

	# Acquire files
	#wget "http://www.angelcode.com/angelscript/sdk/files/angelscript_$ANGELSCRIPT_VERSION.zip" -OutFile angelscript.zip
	wget "http://shepard62fr.free.fr/angelscript-r2521-for-asutils.zip" -OutFile angelscript.zip
	cmd /c 7z x angelscript.zip -o"." -y

	# Override the compiler settings to use a static runtime, so linking with HLE doesn't cause problems
	CMake_GenerateBuildAndInstall @( "-DBUILD_SHARED_LIBS=OFF", "-DCMAKE_USER_MAKE_RULES_OVERRIDE='$ROOT_DIR/cmake/c_flags_overrides.cmake'", "-DCMAKE_USER_MAKE_RULES_OVERRIDE_CXX='$ROOT_DIR/cmake/cxx_flags_overrides.cmake'" ) "sdk/angelscript/projects/cmake"

	cd install

	# We don't need these files, and it reduces cache size
	Remove-Item bin/*
	Remove-Item share/* -recurse

	cd $dependencies_dir
	# Angelscript END

	# Restore to old path
	cd $ROOT_DIR
}
else
{
	Write-Host "Using Cached dependencies" -foregroundcolor green
}
