#include "service_user.h"

HTTP_CODE service_putInfo(const struct http_req* req, struct http_resp* resp)
{
    int ret;

    resp->body = (char*)malloc(1024);
    memset(resp->body, 0, 1024);

    char name[32] = {0};
    int age;
    char numsfx[3] = {0};
    char sex[8] = {0};
    char favorite[128] = {0};
    char buf[128] = {0};

    printf("# service - post body: %s\n", req->body);

    http_getValueFromReqBody(name, 32, "name", req->body);
    http_getValueFromReqBody(buf, 128, "age", req->body);
    age = atoi(buf);
    strcpy(numsfx, "th");
    if(age / 10 != 1)
    {
        if(age % 10 == 1)
            strcpy(numsfx, "st");
        else if(age % 10 == 2)
            strcpy(numsfx, "nd");
        else if(age % 10 == 3)
            strcpy(numsfx, "rd");
    }
    
    http_getValueFromReqBody(buf, 128, "sex", req->body);
    if(strcmp(buf, "male") == 0)
        strcpy(sex, "Mr.");
    else strcpy(sex, "Ms.");
    http_getValueFromReqBody(favorite, 128, "favorite", req->body);

    ret = sprintf(resp->body, 
    "<html><head><title>Info</title></head>"
    "<body><p style=\"text-align: center;text-align: center;font-size:100px\">Hi! %s%s, you will get a %s on your %d%s birthday.</p>"
    "<p style=\"text-align: center;\"><img src=\"/smile.jpg\"></p></body></html>\r\n\r\n",
    sex, name, favorite, age, numsfx);
    resp->contentLength = ret;
    resp->contentType = HTTP_CONTENT_TYPE_HTML;
    
    return HTTP_CODE_OK;
}
