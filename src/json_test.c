#include <stdio.h>
#include <string.h>
#include <json-c/json.h>

int main() {
    const char *json_str = "{\"name\":\"Alice\",\"age\":30,\"score\":95.5}";

    struct json_object *root = json_tokener_parse(json_str);
    if (root == NULL) {
        printf("JSON parse error\n");
        return 1;
    }

    struct json_object *name, *age, *score;
    json_object_object_get_ex(root, "name", &name);
    json_object_object_get_ex(root, "age", &age);
    json_object_object_get_ex(root, "score", &score);

    printf("Name: %s\n", json_object_get_string(name));
    printf("Age: %d\n", json_object_get_int(age));
    printf("Score: %.1f\n", json_object_get_double(score));

    json_object_put(root);
    return 0;
}