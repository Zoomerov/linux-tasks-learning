local group(n) =
  'ИКБО-' + std.toString(n) + '-20';

local student(age, groupNumber, name) = {
  age: age,
  group: group(groupNumber),
  name: name,
};

{
  groups: [
    group(i)
    for i in std.range(1, 24)
  ],

  students: [
    student(19, 4, 'Иванов И.И.'),
    student(18, 5, 'Петров П.П.'),
    student(18, 5, 'Сидоров С.С.'),
    student(19,13, 'Васин Г.А.'),
  ],

  subject: 'Конфигурационное управление',
}
