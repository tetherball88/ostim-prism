# OStim Prism

### New [Prisma UI](https://www.nexusmods.com/skyrimspecialedition/mods/148718) interface for [OStim Standalone - Advanced Adult Animation Framework](https://www.nexusmods.com/skyrimspecialedition/mods/98163)

It is re-imagined interface for OStim, utilizing Prisma UI. While it follows OStim default controls(numpad keys) it has alternative control - cursor.

In Prisma UI cursor appears only in focused mode. While you in focused mode you delegate your controls from game to interface layer. Meaning game won't respond on your mouse move or any hotkeys.

[](./docs/images/1.png | width=1920)

## Hotkeys

New interface use same controls as OStim, all `up`/`right`/`bottom`/`left`/`yes` buttons are same. They will play different roles depends which are is focused right now(`Control buttons` or `Main Menu`) and based on what menu opened right now(`Search`, `Align menu`, `Options`, `Navigation`).

### Special hotkeys

Since alignment menu inputs handle special case for `left`/`right` OStim's hotkeys as decrease/increase value by certain amount of value, to change focus from main menu to control buttons you can use `Tab` or `Esc` hotkeys.

### **NEW** hotkeys

2 new hotkeys related to Prisma UI:
- `left alt` - toggles Prisma cursor
- `right ctrl` - it is disabled by default, toggles webui inspector window(you probably don't need it)

You can change these 2 hotkeys in `OStimPrism.ini` file.

## Sections

The whole interface can be divided into 4 sections:

[](./docs/images/sections.png | width=600)

### Main menu

Interactable section where different menus can be rendered. Depends on what menu you selected, be default shows navigation.

### Control buttons

Here you can toggle between different menus.

#### Controls

To focus on `Control Buttons` from `Main Menu`, usually use `left` hotkey, in case of `Align Menu` use `Esc` or `Tab`. Also you can simply click with cursor if in Prisma UI focus mode.
To select any control buttom hit `yes` hotkey. TO simply switch to whatever currently opened menu use `right` hotkey.

### Status

Small widget which shows different current OStim thread stats: if it has locked controls, is auto-mode, is manual control. In the middle it shows current speed and around widget you can see multiple segments depends on max speed and current speed.

### Excitment bars

Well now technically they are not bars from vanilla OStim, they serve same goal. They show excitement fill for each actor(and now it shows for EACH actor). They have different color scheme based on sex: male-blue, female-pink, other-purple.

Excitement bars also show stamina bar for each actor(small green circle segment), it shows current stamina level. If you use [OEndurance](https://www.nexusmods.com/skyrimspecialedition/mods/61403) it will show changing stamina, otherwise it is always full.

Another one indicator here is cum storage which will be shown only if you use [OCum](https://www.nexusmods.com/skyrimspecialedition/mods/77506).

Now excitement bars also show npc name(you don't need to guess anymore which bar represents which npc) and small addition - orgasmed times.

## Menus

In control buttons you can switch beetween(top to bottom):
- Search
- Align Menu
- Options
- Navigation

### Navigation

Is your main menu which will be shown by default. It is a list of available navigation options you can go from current scene. You can navigate by using 

#### Controls

- `up` - moves selected navigation option up
- `down` - moves selected navigation option up
- `yes` - selects current navigation option and warps your animation to selected scene
- `left` - changes focus from `Main Menu` to `Control buttons`

### Options

This menu looks similar to `Navigation` but its options are special commands which helps to control different aspects of OStim configs. They serve as additional customizable actions to control OStim.

#### Controls

- `up` - moves selected option up
- `down` - moves selected option dows
- `yes` - selects current option
- `left` - changes focus from `Main Menu` to `Control buttons`

### Align Menu

Same as vanilla OStim's align menu, fine tune actors' position, sosBend.
When you open this menu Prisma UI is automatically shown. You can control things with cursor or you can use hotkeys to navigate here.
Unlike OStim vanilla Align menu you can type in inputs numbers so you don't need to hit `right` hotkey 180 times to rotate actor 180deg

#### Controls

- `up` - moves up focused attribute you want to change
- `down` - moves down focused attribute you want to change
- `left` - decreases by certain amount selected value
- `right` - increases by certain amount selected value

To exit this menu use `Esc` or `Tab` or click `Close` button.

### Search

Type in search input scene name or id you are looking for. It will show matched scenes in alphabetical order.
You can use `ctrl+c` and `ctrl+v` to copy/paste

WARNING. Some SKS plugins use hotkeys. Neither vanilla OStim menus nor OStim Prism can disable them. There might be a case when you type your scene name you can hit hotkey which for example will show Skyrim Vanilla message or input window. It will disrupt Prisma UI focus. To exit it try this action:
- If Prisma UI cursor is show hit `left alt` to hide cursor and switch controls focus from Prisma UI back to game.
- Do something to hide whatever you are trying to hide(for example if it is some kind of dialogue window you can close it)
- Hit `left alt` again to regain focus for Prisma UI and continue your searching activities. You can type into input only when Prisma UI is focused!

- `up` - moves up selected active found search results 
- `down` - moves down selected active found search results 
- `yes` - selects current search item and warps your animation to selected scene
- `left` - changes focus from `Main Menu` to `Control buttons`
