% Automatically generated MiniZinc model

var 0..2: foo;
var 0..1: left;
var 0..1: right;
var 0..2: shared;
var 0..2: target;

constraint (foo = 1 \/ foo = 2);
constraint (target = 2);

constraint (foo = 2) -> (left = 1);
constraint (foo = 2) -> (right = 1);
constraint (left = 1) -> (shared = 1 \/ shared = 2);
constraint (right = 1) -> (shared = 1);
constraint (shared = 1) -> (target = 1);

solve minimize sum([bool2int(foo != 0), bool2int(left != 0), bool2int(right != 0), bool2int(shared != 0), bool2int(target != 0)]);

array[0..2] of string: foo_versions = array1d(0..2, ["not installed", "1.0.0", "1.1.0"]);
array[0..1] of string: left_versions = array1d(0..1, ["not installed", "1.0.0"]);
array[0..1] of string: right_versions = array1d(0..1, ["not installed", "1.0.0"]);
array[0..2] of string: shared_versions = array1d(0..2, ["not installed", "1.0.0", "2.0.0"]);
array[0..2] of string: target_versions = array1d(0..2, ["not installed", "1.0.0", "2.0.0"]);

output [
  "root = 1.0.0\n",
  "foo = " ++ foo_versions[fix(foo)] ++ "\n",
  "left = " ++ left_versions[fix(left)] ++ "\n",
  "right = " ++ right_versions[fix(right)] ++ "\n",
  "shared = " ++ shared_versions[fix(shared)] ++ "\n",
  "target = " ++ target_versions[fix(target)] ++ "\n",
];