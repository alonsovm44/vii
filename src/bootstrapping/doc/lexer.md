# Documentation for Vii Self-Hosting Lexer

from line 6 to 45{
    TK_KW defined as an immutable list and set each 38 keyword value

}

from line 47 to 86{
    KW_KIND, aka keyword kind, defined as an immutable list and assigned each value from vii.vii table
}

Then we define KW_MAP dictionary for a O(1) lookup 
```vii
KW_MAP = dict
i = 0
while i < len TK_KW
  KW_MAP key (TK_KW at i) (KW_KIND at i)
  i = i + 1
```

**is_digit**
A function to verify if a character is a digit.
We then define `is_digit` function. It takes c as input.
o = ord c <-we cast c into ascii number and assign it to "o"
We then check if o is between ASCII 48 and ASCII 57, out 1 if true and 0 if false

**is_alpha**
A function to verify if a character is an a lower caps alphabet letter or a capped alphabet letter or an underscore _, returns 1 on either cases and 0 on the other

**is_alnum** 
A function that checks if the character is alphanumeric, returns 0 if neither and 1 if either 

**get_kw_kind**
