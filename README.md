# Bing Wallpaper
A small program to set the daily Bing wallpaper.

I had to write this because [Auto Dark Mode](https://github.com/AutoDarkMode/Windows-Auto-Night-Mode) would not play ball with [Microsoft Bing Wallpaper](https://www.bing.com/apps/wallpaper) app.

## Build
Use [CPM](https://github.com/neacsum/cpm) to build it.

1. Download [cpm program](https://github.com/neacsum/cpm/releases/latest).

2. Run the following command:
```text
cpm -u git@github.com:neacsum/bing_wallpaper.git bing_wallpaper
```

## Using
If you have Auto Dark Mode installed, edit the `scripts.yaml` file to contain something like:
```yaml
Enabled: true
Component:
  Scripts:
  - Name: Bing Wallpaper
    Command: wallpaper
    AllowedSources: [Any]
```