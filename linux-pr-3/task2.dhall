let group =
      \(n : Natural) ->
        "ИКБО-${Natural/show n}-20"

let Student =
      { age : Natural
      , group : Text
      , name : Text
      }

let student =
      \(age : Natural) ->
      \(groupNumber : Natural) ->
      \(name : Text) ->
        { age = age
        , group = group groupNumber
        , name = name
        }

let GroupState =
      { next : Natural
      , groups : List Text
      }

let addGroup =
      \(state : GroupState) ->
        { next = state.next + 1
        , groups = state.groups # [ group state.next ]
        }

let generated =
      Natural/fold
        24
        GroupState
        addGroup
        { next = 1
        , groups = [] : List Text
        }

in  {
      groups = generated.groups,

      students =
        [ student 19 4 "Иванов И.И."
        , student 18 5 "Петров П.П."
        , student 18 5 "Сидоров С.С."
        , student 19 13 "Васин Г.А"
        ],

      subject = "Конфигурационное управление"
    }
