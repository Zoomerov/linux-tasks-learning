% Индексы версий:
% menu:     1=1.0.0, 2=1.1.0, ..., 6=1.5.0
% dropdown: 1=1.8.0, 2=2.0.0, ..., 5=2.3.0
% icons:    1=1.0.0, 2=2.0.0

var 1..6: menu;
var 1..5: dropdown;
var 1..2: icons;

% root требует icons < 2.0.0
constraint icons = 1;

% menu >= 1.1.0 требует dropdown >= 2.0.0
constraint (menu >= 2) -> (dropdown >= 2);

% menu 1.0.0 требует старый dropdown
constraint (menu = 1) -> (dropdown = 1);

% dropdown >= 2.0.0 требует icons >= 2.0.0
constraint (dropdown >= 2) -> (icons = 2);

solve satisfy;

array[1..6] of string: menu_versions =
    ["1.0.0", "1.1.0", "1.2.0", "1.3.0", "1.4.0", "1.5.0"];

array[1..5] of string: dropdown_versions =
    ["1.8.0", "2.0.0", "2.1.0", "2.2.0", "2.3.0"];

array[1..2] of string: icons_versions =
    ["1.0.0", "2.0.0"];

output [
    "menu = " ++ menu_versions[fix(menu)] ++ "\n",
    "dropdown = " ++ dropdown_versions[fix(dropdown)] ++ "\n",
    "icons = " ++ icons_versions[fix(icons)] ++ "\n"
];
