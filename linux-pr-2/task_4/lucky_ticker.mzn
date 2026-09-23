include "alldifferent.mzn";

array[1..6] of var 0..9: d;

constraint all_different(d);

constraint d[1] + d[2] + d[3] =
           d[4] + d[5] + d[6];

solve minimize d[1] + d[2] + d[3];

output [
  "ticket = ",
  show(d[1]), show(d[2]), show(d[3]),
  show(d[4]), show(d[5]), show(d[6]),
  "\nsum = ",
  show(d[1] + d[2] + d[3])
];
