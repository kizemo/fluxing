#!/usr/bin/env pwsh
param( [string]$tag, [string]$basetag, [switch]$updatelog, [switch]$h)
# -tag [new_tag], new tag shall be in format of ^\d+\.\d+\.\d+$ and greater than
#     the current one or $basetag
# -basetag [basetag name], specify the base tag(which shall be in format of ^\d+\.\d+\.\d+$),
#     default to be the latest number format tag name
# -updatelog, update CHANGELOG.md if this switch used
# -h, show help info
# Fluxing 分支常量 (取代上游 rime.github.io 链接)
$FLUXING_RELEASE_NOTES = "https://www.aiec.fun/pinyin/release-notes.html"
$FLUXING_DOWNLOAD_URL = "https://www.aiec.fun/pinyin/download.php?file=fluxing-"
$repoRoot = (Get-Location).Path
function faild_exit {
  Write-Host 
  Write-Host "Orz :( bump version failed!"
  Write-Host "we will revert the modification by command git checkout . "
  Write-Host 
  & git checkout .
  exit 1
}
# get old version info from file, esp from build.bat
function get_old_version{
  param ([string]$filePath)
  $fileContent = Get-Content -Path $filePath;
  $version = @{};
  foreach ($line in $fileContent) {
    if ($version["major"] -eq $null -and $line -match 'VERSION_MAJOR=(\d+)') {
      $version["major"] = $matches[1];
    }
    if ($version["minor"] -eq $null -and $line -match 'VERSION_MINOR=(\d+)') {
      $version["minor"] = $matches[1];
    }
    if ($version["patch"] -eq $null -and $line -match 'VERSION_PATCH=(\d+)') {
      $version["patch"] = $matches[1];
    }
    if ($version["major"] -ne $null -and $version["minor"] -ne $null -and $version["patch"] -ne $null) {
      break;
    }
  }
  $version['str'] = $version["major"] + "." + $version["minor"] + "." + $version["patch"];
  return $version;
}
# parse new version info
function parse_new_version {
  param([string]$new_version)
  $version = @{};
  if ($new_version -match "^(\d+)\.(\d+)\.(\d+)$") {
    $version['major'] = $matches[1];
    $version['minor'] = $matches[2];
    $version['patch'] = $matches[3];
  }
  return $version;
}
# update change log, work like clog-cli
function update_changelog {
  param( [string]$new_tag, [string]$old_tag)
  $commits = git log "$old_tag..HEAD" --pretty=format:"%s ([%an](https://github.com/kizemo/fluxing/commit/%H))";
  # group commit logs
  $groupedCommits = @{
    build = @(); ci = @(); fix = @(); feat = @(); docs = @(); style = @();
    refactor = @(); test = @(); chore = @(); commit = @();
  };
  $groupedTitle = @{
    build = "Builds";
    ci = "Continuous Integration";
    fix = "Bug Fixes";
    feat = "Features";
    docs = "Documents";
    style = "Code Style";
    refactor = "Code Refactor";
    test = "Tests";
    chore = "Chores";
    commit = "Commits";
  };
  foreach ($commit in $commits) {
    if ($commit -match "^((?i)(build|ci|fix|feat|docs|style|refactor|test|chore)(\([^\)]+\))?):") {
      $prefix = $matches[2].ToLower();
      $groupedCommits[$prefix] += $commit;
    } else {
      $groupedCommits["commit"] += $commit;
    }
  }
  $changelog = "";
  foreach ($group in $groupedCommits.Keys) {
    if ($groupedCommits[$group].Count -gt 0) {
      $changelog += "`n#### " + $groupedTitle[$group] + "`n"
        foreach ($commit in $groupedCommits[$group]) {
          $msg = $commit -replace "^" + $group + ":", '';
          $changelog += "* $msg`n";
        }
    }
  }
  $currentDateTime = Get-Date -Format "yyyy-MM-dd";
  $fileContent = [System.Text.Encoding]::UTF8.GetString([System.IO.File]::ReadAllBytes("CHANGELOG.md"));
  $contentAdd = "<a name=`"$new_tag`"></a>`n" ;
  $contentAdd += "## [$new_tag](https://github.com/kizemo/fluxing/compare/$old_tag...$new_tag)($currentDateTime)`n" ;
  # if $new_tag.txt exists, add the content to changelog
  $new_tag_file = "$new_tag.txt";
  if (Test-Path -Path $new_tag_file) {
    $new_tag_content = Get-Content -Path $new_tag_file -Raw -Encoding UTF8;
    $contentAdd += "`n" + $new_tag_content + "`n";
  }

  $contentAdd += $changelog;
  Write-Host "`n" + $contentAdd + "`n"
  $fileContent = $contentAdd + "`n" + $fileContent;
  [System.IO.File]::WriteAllBytes("CHANGELOG.md", [System.Text.Encoding]::UTF8.GetBytes($fileContent));
}
# replace string with regex pat
function replace_str {
  param( [string]$filePath, [string]$pat_orig, [string]$pat_replace)
  # L01: 字节级 UTF-8 读写。上游用 Get-Content -Raw / Out-File (都不带 -Encoding),
  # 在 PS 5.1 下按 ANSI(GBK) 处理, 会把 appcast 里的中文烧成乱码。
  # 字节级读写同时原样保留 BOM 与 CRLF (L09)。
  $bytes = [System.IO.File]::ReadAllBytes($filePath)
  $fileContent = [System.Text.Encoding]::UTF8.GetString($bytes)
  $fileContent = [regex]::Replace($fileContent, $pat_orig, $pat_replace)
  [System.IO.File]::WriteAllBytes($filePath, [System.Text.Encoding]::UTF8.GetBytes($fileContent))
}
# update xml file
function update_xml {
  param([string]$filePath, [string]$tag)
  $CurrentCulture = [System.Globalization.CultureInfo]::InvariantCulture
  $localTime = Get-Date
  $currentDateTime = $localTime.ToString("ddd, dd MMM yyyy HH:mm:ss zz00")
  # Fluxing: 只改"最新那个 item"(第一个 <item>), 历史 item 一律不动。
  # 上游的正则 "\d+\.\d+\.\d+" 会替换整个文件里的所有版本号, 在多 item 的
  # Fluxing appcast 上会把历史版本全部改成同一个 tag。
  # 锚点统一写成 (?s)(<item>.*?...) —— 惰性匹配只命中第一个 <item>。
  replace_str -filePath "$filePath" `
    -pat_orig "(?s)(<item>.*?<title>.*?)(\d+\.\d+\.\d+)(.*?</title>)" `
    -pat_replace ('$1' + $tag + '$3')
  replace_str -filePath "$filePath" `
    -pat_orig "(?s)(<item>.*?<sparkle:releaseNotesLink>)[^<]*(</sparkle:releaseNotesLink>)" `
    -pat_replace ('$1' + $FLUXING_RELEASE_NOTES + '$2')
  replace_str -filePath "$filePath" `
    -pat_orig "(?s)(<item>.*?<pubDate>).*?(</pubDate>)" `
    -pat_replace ('$1' + $currentDateTime + '$2')
  replace_str -filePath "$filePath" `
    -pat_orig '(?s)(<item>.*?<enclosure url=")[^"]*(")' `
    -pat_replace ('$1' + $FLUXING_DOWNLOAD_URL + $tag + '-installer.exe$2')
  replace_str -filePath "$filePath" `
    -pat_orig '(?s)(<item>.*?sparkle:version=")[^"]*(")' `
    -pat_replace ('$1' + $tag + '$2')
  replace_str -filePath "$filePath" `
    -pat_orig '(?s)(<item>.*?sparkle:shortVersionString=")[^"]*(")' `
    -pat_replace ('$1' + $tag + '$2')
  # length 只有在本仓库真有该安装包时才是真的; 否则删掉这个属性 ——
  # 留着旧 length 会让 Sparkle 按旧尺寸判定下载失败。
  $installer = Join-Path $repoRoot ("release/fluxing-{0}-installer.exe" -f $tag)
  if (Test-Path -Path $installer) {
    $installerLength = (Get-Item -Path $installer).Length
    replace_str -filePath "$filePath" `
      -pat_orig '(?s)(<item>.*?length=")[^"]*(")' `
      -pat_replace ('$1' + $installerLength + '$2')
  } else {
    replace_str -filePath "$filePath" `
      -pat_orig '(?s)(<item>.*?)\s+length="[^"]*"' `
      -pat_replace '$1'
  }
  Write-Host "$filePath updated"
}
# update bat file
function update_bat {
  param([string]$filePath, [string]$tag)
  $new_version = parse_new_version -new_version $tag
  replace_str -filePath $filePath -pat_orig "(?<=VERSION_MAJOR=)\d+" $new_version['major']
  replace_str -filePath $filePath -pat_orig "(?<=VERSION_MINOR=)\d+" $new_version['minor']
  replace_str -filePath $filePath -pat_orig "(?<=VERSION_PATCH=)\d+" $new_version['patch']
  Write-Host "$filePath updated"
}
# test if cwd is weasel root
function is_weasel_root() {
  return (Test-Path -Path "weasel.sln") -and (Test-Path -Path ".git")
}
###############################################################################
# program now started
if ($h -or (-not $tag)) {
  $info = @(
 "-tag [new_tag]`n`tnew tag shall be in format of ^\d+\.\d+\.\d+$ and greater than the current one or `$basetag",
 "-basetag [basetag name]`n`tspecify the base tag(which shall be in format of ^\d+\.\d+\.\d+$), default to be the latest number format tag name",
 "-updatelog`n`tupdate CHANGELOG.md if this switch used",
 "-h`n`tshow help info"
 )
 Write-Host ($info -join "`n");
 exit 0;
}
if (-not (is_weasel_root)) {
  Write-Host "Current directory is: $(Get-Location), it's not the weasel root directory"
  Write-Host "please run ``update/bump_version.ps1`` with parameters under the weasel root in Powershell";
  Write-Host "or run ``pwsh update/bump_version.ps1`` with parameters under weasel root in shell if yor're running Mac OS or Linux"
  exit 1;
}
# tag name not match rule, exit
if (-not ($tag -match '^\d+\.\d+\.\d+$')) {
  Write-Host "tag name not match rule '^\d+\.\d+\.\d+$'";
  Write-Host "please recheck it";
  exit 1;
}
# get old version
$old_version = get_old_version -filePath "build.bat";
# get new version
$new_version = parse_new_version -new_version $tag
if ($basetag -eq "") {
  $basetag = $old_version["str"]
}
if ($basetag -notmatch "^\d+\.\d+\.\d+$") {
  Write-Host "basetag format is not correct, it shall be in format of '^\d+\.\d+\.\d+$'"
  Write-Host "please recheck it";
  exit 1;
}

# check new version is greater than the current one
if ($tag -eq $old_version["str"]) {
  Write-Host "the new tag: $tag is the same with the old one:" $old_version["str"];
  exit 1;
} elseif ($tag -lt $old_version["str"]) {
  Write-Host "$tag is older version than " $old_version['str']", please recheck your target version.";
  exit 1;
}
Write-Host "Bumping version from $($old_version['str']) to $tag ..."
# update xml files
update_xml -filePath "update/appcast.xml" -tag "$tag";
if ($?) { } else { faild_exit }
update_xml -filePath "update/testing-appcast.xml" -tag "$tag";
if ($?) { } else { faild_exit }
# update bat files
update_bat -filePath "build.bat" -tag "$tag";
if ($?) { } else { faild_exit }
update_bat -filePath "xbuild.bat" -tag "$tag";
if ($?) { } else { faild_exit }
#update CHANGELOG.md
if ($updatelog) {
  update_changelog -new_tag $tag -old_tag $basetag;
  if ($?) { } else { faild_exit }
  & git add CHANGELOG.md
  Write-Host "CHANGELOG.md updated"
}
#commit changes and make a new tag
$release_message = "chore(release): $tag :tada:"
$release_tag = $tag
& git add update/*.xml build.bat xbuild.bat
& git commit --message $release_message
& git tag --annotate $release_tag --message $release_message
