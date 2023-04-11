# We're expecting the name in kebab case, will the convert it to camel and upper snake case
param(
    [parameter(Mandatory=$true)]
    [string]$solutionPath,

    [parameter(Mandatory=$true)]
    [string]$projectPath,

    [parameter(Mandatory=$true)]
    [string]$skewerCaseStateName
)

Write-Output("Generating files...");

# generate the files to add
$lowerTemp = '<temp>'
$upperTemp = $lowerTemp.ToUpper()

$first, $rest = $skewerCaseStateName -Replace '[^0-9A-Z]', ' ' -Split ' ',2
$camelCase = $first.ToLower() + (Get-Culture).TextInfo.ToTitleCase($rest) -Replace ' '

$upperSnakeCase = $skewerCaseStateName.Replace('-','_').ToUpper();

#need the full path here instead of the local path

$baseGamePath = (Resolve-Path "..\Game\Game").ToString()

$headerTemplateFilePath = "..\Templates\state_header.txt"
$sourceTemplateFilePath = "..\Templates\state_source.txt"
$headerOutFilePath = $baseGamePath + "\" + $camelCase + "State.h"
$sourceOutFilePath = $baseGamePath + "\" + $camelCase + "State.c"

$headerExists = Test-Path $headerOutFilePath -PathType Leaf
$sourceExists = Test-Path $sourceOutFilePath -PathType Leaf
if ($headerExists -Or $sourceExists)
{
    Do {
        $answer = Read-Host -Prompt "State already exists, overwrite? (y/n)"
    }
    Until ($answer -eq "y" -or $answer -eq "n")

    if ($answer -eq "n")
    {
        exit
    }
}

Copy-Item $headerTemplateFilePath $headerOutFilePath
Copy-Item $sourceTemplateFilePath $sourceOutFilePath

(Get-Content $headerOutFilePath).replace($upperTemp, $upperSnakeCase) | Set-Content $headerOutFilePath
(Get-Content $sourceOutFilePath).replace($upperTemp, $upperSnakeCase) | Set-Content $sourceOutFilePath

(Get-Content $headerOutFilePath).replace($lowerTemp, $camelCase) | Set-Content $headerOutFilePath
(Get-Content $sourceOutFilePath).replace($lowerTemp, $camelCase) | Set-Content $sourceOutFilePath


#add the files to the current project

Write-Output("Adding files to project...");

$IDE = New-Object -ComObject VisualStudio.DTE

Write-Output("Opening solution...");
$IDE.Solution.Open($solutionPath)

#Write-Output($solutionPath)
#Write-Output($projectPath)

#need the full path here instead of the local path
#Write-Output($headerOutFilePath)
#Write-Output($sourceOutFilePath)

Write-Output("Finding project...");
$project = $IDE.Solution.Projects | ? { $_.FullName -eq $projectPath }

Write-Output("Finding filters...");
#Need to go down through the project items until we find where we want to add things
$headerFilterPath = @("Header Files", "Game", "States")
$sourceFilterPath = @("Source Files", "Game", "States")

$headerFilter = $project
for($i = 0; $i -lt $headerFilterPath.Count; $i++)
{
    $headerFilter = $headerFilter.ProjectItems | ? { $_.Name -eq $headerFilterPath[$i] }
    #Write-Output($headerFilter.Name)
}

$sourceFilter = $project
for($i = 0; $i -lt $sourceFilterPath.Count; $i++)
{
    $sourceFilter = $sourceFilter.ProjectItems | ? { $_.Name -eq $sourceFilterPath[$i] }
    #Write-Output($sourceFilter.Name)
}

Write-Output("Adding files to filters...")
$headerFilter.ProjectItems.AddFromFile($headerOutFilePath) | Out-Null
$sourceFilter.ProjectItems.AddFromFile($sourceOutFilePath) | Out-Null
Write-Output("Saving project...")
$project.Save()

$IDE.Quit()

#Read-Host -Prompt "Press any key to continue or CTRL+C to quit" 