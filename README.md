# rlm_mongodb_ops

### Build instructions
Separate build Using CMAKE:
```bash
mkdir build
cd build
cmake ..
make
```

For in-tree build just copy source files to `src/modules/rlm_mongodb_ops` and add module in Make.inc MODULES section.

### Config examples

```
# Upsert example
mongodb_ops set_example {
    server = "mongodb://127.0.0.1:27017/"
    pool_size = 1
    action = "set"
    db = "sessions"
    collection = "sessions"
    search_query = "{\"ip\":\"%{reply:Framed-Ip-Address}\"}"
    sort_query = ""
    update_query = "{ \
        \"\\$set\": { \
            \"ip\": \"%{reply:Framed-Ip-Address}\", \
            \"nas_ip\": \"%{Nas-Ip-Address}\", \
            \"login\": \"%{User-Name}\", \
        } \
    }"
    remove = no
    upsert = yes
}

```
