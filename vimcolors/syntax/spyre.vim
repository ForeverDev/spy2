syntax keyword spyreKeyword  func return if while do for repeat until
syntax keyword spyreKeyword  continue break defer struct new
syntax keyword spyreKeyword  null true false
syntax keyword spyreType real string null strong weak new
syntax match spyreOperator "\v\*"
syntax match spyreOperator "\v/"
syntax match spyreOperator "\v\+"
syntax match spyreOperator "\v-"
syntax match spyreOperator "\v\?"
syntax match spyreOperator "\v\*\="
syntax match spyreOperator "\v/\="
syntax match spyreOperator "\v\+\="
syntax match spyreOperator "\v-\="
syntax match spyreOperator "\v:"
syntax match spyreOperator "\v\="
syntax match spyreComment "\v//.*$"
syntax match spyreString "\v\".*\""
syntax match spyreNumber "\v<\d+>"
syntax match spyreNumber "\v<\d+\.\d+>"
syntax match spyreNumber "\v<\d*\.?\d+([Ee]-?)?\d+>"
syntax match spyreNumber "\v<0x\x+([Pp]-?)?\x+>"
syntax match spyreNumber "\v<0b[01]+>"
syntax match spyreNumber "\v<0o\o+>"

highlight link spyreComment Comment
highlight link spyreOperator Operator
highlight link spyreKeyword Keyword
highlight link spyreNumber  Number
highlight link spyreString String
highlight link spyreType Type
