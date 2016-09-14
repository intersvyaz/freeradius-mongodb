# FreeRADIUS 3.x MongoDB module

Simple module for performing MongoDB get/set operations.

### Config examples

```
# Upsert example
mongodb mongodb_set_example {
    # Perform set action
    action = "set"
    
    # Server string
    # Read more about connection string format here:
    # [https://docs.mongodb.com/manual/reference/connection-string/]
    server = "mongodb://127.0.0.1:27017/"
    
   # Connection pool
    pool {
        start = ${thread[pool].start_servers}
        min = ${thread[pool].min_spare_servers}
        max = ${thread[pool].max_servers}
        spare = ${thread[pool].max_spare_servers}
        uses = 0
        lifetime = 0
        idle_timeout = 60
    }
    
    # Database name (support attributes substitution)
    db = "sessions"
    
    # Collection name (support attributes substitution)
    collection = "sessions"
    
    # Search query (support attributes substitution)
    search_query = "{\"ip\":\"%{reply:Framed-Ip-Address}\"}"
    
    # Sort query (optional, support attributes substitution)
    sort_query = ""
    
    # Update query (optional, support attributes substitution)
    update_query = "{ \
        \"$set\": { \
            \"ip\": \"%{reply:Framed-Ip-Address}\", \
            \"nas_ip\": \"%{Nas-Ip-Address}\", \
            \"login\": \"%{User-Name}\", \
        } \
    }"
    
    # Perform remove (optional, default is "no")
    remove = no
    
    # Perform upsert (optional, default is "no")
    upsert = no
}

```
