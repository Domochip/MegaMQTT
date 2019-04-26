#include "WebServer.h"

WebServer::WebServer() : _webServer(80)
{
    _webServer.begin();
}

void WebServer::Begin(WebServerCallbackFunc callback)
{
    _callback = callback;
    _webServer.begin();
};

void WebServer::Run()
{
    //take and check if a webClient is there
    EthernetClient webClient = _webServer.available();
    if (!webClient)
        return;

    //if it's connected
    if (webClient.connected())
    {
        //-------------------Receive and parse his request-------------------
        bool isRequestHeaderFinished = false;
        bool isPOSTRequest = false;
        String requestURI; //URL requested
        String requestBoundary;
        String fileContent;

        while (webClient.available() && !isRequestHeaderFinished)
        {
            String reqLine = webClient.readStringUntil('\n');

            //DEBUG
            // Serial.print(F("->"));
            // Serial.println(reqLine);
            // Serial.print(F("--+ Length : "));
            // Serial.println(reqLine.length());

            //Parse method and URI
            if (reqLine.endsWith(F(" HTTP/1.1\r")))
            {
                if (reqLine.startsWith(F("GET ")))
                    requestURI = reqLine.substring(4, reqLine.length() - 10);
                if (reqLine.startsWith(F("POST ")))
                {
                    isPOSTRequest = true;
                    requestURI = reqLine.substring(5, reqLine.length() - 10);
                }
            }

            //Parse boundary (if file is POSTed)
            if (isPOSTRequest && reqLine.startsWith(F("Content-Type: multipart/form-data; boundary=")))
            {
                requestBoundary = reqLine.substring(44, reqLine.length() - 1);
            }

            //Does request Header is finished
            if (reqLine.length() == 1 && reqLine[0] == '\r')
                isRequestHeaderFinished = true;
        }

        //Try to receive file content (only for POST request)
        bool isFileContentReceived = false;
        if (isPOSTRequest)
        {
            //look for boundary
            if (webClient.find((char *)requestBoundary.c_str()))
            {
                //then go to next empty line (so skip it)
                webClient.find((char *)"\r\n\r\n");

                //complete requestBoundary to find its end
                requestBoundary = "--" + requestBoundary + '\r';

                //and finally read file content until boundary end string is found
                while (webClient.available() && !isFileContentReceived)
                {
                    //read content line
                    String dataLine = webClient.readStringUntil('\n');
                    //if it doesn't match end of boundary
                    if (dataLine.compareTo(requestBoundary))
                        fileContent += dataLine + '\n'; //add it to fileContent with removed \n
                    else
                        isFileContentReceived = true; //else end of file content is there
                }
            }
        }

        //DEBUG
        Serial.print(F("Request Method : "));
        Serial.println(isPOSTRequest ? F("POST") : F("GET"));
        Serial.print(F("Request URI : "));
        Serial.println(requestURI);
        Serial.print(F("Request Boundary : "));
        Serial.println(requestBoundary);
        Serial.print(F("Request File received : "));
        Serial.println(isFileContentReceived ? F("YES") : F("NO"));
        Serial.print(F("Request File content : "));
        Serial.println(fileContent);

        //-------------------Execute CallBack to make request answer-------------------
        if (_callback)
            _callback(webClient, isPOSTRequest, requestURI.c_str(), isFileContentReceived, fileContent.c_str());
    }
    webClient.stop();
};