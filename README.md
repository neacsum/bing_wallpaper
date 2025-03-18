# Bing Wallpaper
A small program to set the daily Bing wallpaper.

[Auto Dark Mode](https://github.com/AutoDarkMode/Windows-Auto-Night-Mode) is an
app that switches between Windows light and dark themes, setting the light theme
during the day and dark theme at night. There are more options but that's the
gist of it.  

[Microsoft Bing Wallpaper](https://www.bing.com/apps/wallpaper)
app changes the desktop wallpaper daily with some really pretty images.
Unfortunately, it checks if the wallpaper has been changed and if it has been,
it stops updating it. I couldn't make the two applications work together and
that's how this little program came to exist.

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

## How it Works
The program downloads a JSON file describing the current Bing wallpaper and
changes the current wallpaper to the Bing wallpaper. 

The ADM (Auto Dark Mode) script invokes the program but, unfortunately, it sets
Windows theme __after__ calling the script. To avoid this problem, the second
instance of the program (the one invoked by ADM), just sends a message to the
running instance. This one, in turn, waits a bit (0.5 sec seems OK) and then
reverts the wallpaper to the Bing wallpaper.