% root 1.0.0 зависит от foo ^1.0.0 и target ^2.0.0
% foo 1.1.0 зависит от left ^1.0.0 и right ^1.0.0
% foo 1.0.0 не имеет зависимостей
% left 1.0.0 зависит от shared >=1.0.0
% right 1.0.0 зависит от shared <2.0.0
% shared 2.0.0 не имеет зависимостей
% shared 1.0.0 зависит от target ^1.0.0
% target 2.0.0 и 1.0.0 не имеют зависимостей


% foo:
% 1 = 1.0.0
% 2 = 1.1.0
var 1..2: foo;

% left:
% 0 = не установлен
% 1 = 1.0.0
var 0..1: left;

% right:
% 0 = не установлен
% 1 = 1.0.0
var 0..1: right;

% shared:
% 0 = не установлен
% 1 = 1.0.0
% 2 = 2.0.0
var 0..2: shared;

% target:
% 1 = 1.0.0
% 2 = 2.0.0
var 1..2: target;


% root требует target ^2.0.0
constraint target = 2;

% root требует foo ^1.0.0
% обе доступные версии foo подходят:
% 1.0.0 и 1.1.0


% если выбран foo 1.1.0,
% нужны left 1.0.0 и right 1.0.0
constraint (foo = 2) -> (left = 1 /\ right = 1);

% foo 1.0.0 не имеет зависимостей
constraint (foo = 1) -> (left = 0 /\ right = 0);


% left 1.0.0 требует shared >= 1.0.0
constraint (left = 1) -> (shared >= 1);

% right 1.0.0 требует shared < 2.0.0
% значит подходит только shared 1.0.0
constraint (right = 1) -> (shared = 1);


% если left и right не установлены,
% shared тоже не требуется
constraint (left = 0 /\ right = 0) -> (shared = 0);


% shared 1.0.0 требует target ^1.0.0
constraint (shared = 1) -> (target = 1);

% shared 2.0.0 зависимостей не имеет


solve satisfy;


array[1..2] of string: foo_v =
    ["1.0.0", "1.1.0"];

array[0..1] of string: lr_v =
    array1d(0..1, ["not installed", "1.0.0"]);

array[0..2] of string: shared_v =
    array1d(0..2, ["not installed", "1.0.0", "2.0.0"]);

array[1..2] of string: target_v =
    ["1.0.0", "2.0.0"];


output [
    "foo = " ++ foo_v[fix(foo)] ++ "\n",
    "left = " ++ lr_v[fix(left)] ++ "\n",
    "right = " ++ lr_v[fix(right)] ++ "\n",
    "shared = " ++ shared_v[fix(shared)] ++ "\n",
    "target = " ++ target_v[fix(target)] ++ "\n"
];
