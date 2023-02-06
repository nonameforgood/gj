class GJString;

bool HttpGet(const char *get, GJString &response);

bool HttpPost(const char *uri, const char *data, uint32_t dataSize, GJString &response);