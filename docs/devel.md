# Developing

# Using the tpl_* functions 

```c
#include <stdio.h>      // for fprintf, free, puts
#include <stdlib.h>     // for strdup
#include "template.h"   // for tpl_free, tpl_register, tpl_render

int main(int argc, char *argv[]) {
    char *value = strdup("the value");
    tpl_register("my_data", &value);
    char *rendered = tpl_render("I am showing you {{ my_data }}.");
    if (rendered) {
        puts(rendered);
        free(rendered);
    } else {
        fprintf()
    }
    tpl_free();
}
```

`tpl_register` accepts an address to a heap-allocated pointer of type `char`. One cannot pass the address of a stack-allocated character string, because chances are reasonably high that your C compiler detect this condition and throw an _incompatible pointer_ error. You may, however, register a pointer to a string that has been allocated on the stack (see example).

```c
// Invalid (stack)
char value[255];

// Invalid (stack)
char value[] = "the value";

// Valid (pointer to stack)
char value_s[] = "the value";
char *value = value_s;

// Valid (heap)
char *value = calloc(255, sizeof(*value));
strcpy(value, "the value");

// Valid (heap)
char *value = strdup("the value");
```

The `tpl_render` function parses an input string and replaces any references encapsulated by double curly braces (`{{}}`) with the _current_ value of a registered template variable. Empty references and undefined variables are ignored, however whitespace surrounding the reference will be preserved in the result. If an unrecoverable error occurs while rendering this function returns `NULL`.

The following illustrates this effect:
```c
char *abc = strdup("ABC");
tpl_register("abc", &abc);
char *rendered = tpl_render("{{}} {{ undefined_var }} I know my {{ abc }}'s!");
// Result: "  I know my ABC's!"
//          ^^ whitespace
free(rendered);
tpl_free();
```

One should consider using the `normalize_space` function to remove undesired whitespace from the rendered output.

```c
#include <stdio.h> // for fprintf, stderr
#include "str.h"   // for normalize_space

// ...
char *rendered = tpl_render("{{}} {{ undefined_var }} I know my {{ abc }}'s!");
if (rendered) {
    // Remove leading, trailing, and repeated whitespace
    normalize_space(rendered);
} else {
    fprintf(stderr, "unable to render input string\n");
    exit(1);
}
free(rendered);
tpl_free();
// Result: "I know my ABC's!"
```

Most calls to `tpl_render` use data read from a file. The examples below should clarify how to achieve this.

Template file: **index.html.in**

```html
<!DOCTYPE html>
<html lang="en">
    <head>
        <title>{{ site_title }}</title>
    </head>
    <body>
        {{ site_body_text }}
        <br/>
        {{ site_footer }}
    </body>
</html>
```

- Using the standard library

```c
#include <stdio.h>      // for fgets
#include <string.h>
#include <stdlib.h>
#include "template.h"   // for tpl_render

struct Site {
    char *title;
    char *body;
    char *footer;
} site;

void site_setup_template() {
    tpl_register("site_title", &site.title);
    tpl_register("site_body", &site.body);
    tpl_register("site_footer", &site.footer);
}

void site_setup_data() {
    site.title = strdup("My static site");
    site.body = strdup("This is the body.");
    site.footer = strdup("Generated with tpl_render()");
}

int main(int argc, char *argv[]) {
    char line[BUFSIZ] = {0};
    char *filename = argv[1];
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror(filename);
        exit(1);
    }
    
    while (fgets(line, sizeof(line) - 1, fp) != NULL) {
        char *rendered = tpl_render(line);
        if (rendered) {
            normalize_space(rendered);
            printf("%s", rendered);
        }
    }
    fclose(fp);
}
```

- Using file_readlines

```c

#include "util.h"
```

