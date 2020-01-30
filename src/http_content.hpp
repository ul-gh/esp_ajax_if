// Display the HTML web page
// For the ESP32, immutable data is automatically stored in FLASH, on the
// ESP8266 whis would be:
// #include <Arduino.h>
// const char INDEX_HTML[] PROGMEM = ...
constexpr const char index_html[] = "";
//constexpr const char index_html[] =
//    "<!DOCTYPE html>"
//    "<html>"
//    "<head>"
//    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
//    "<meta charset=\"UTF-8\">"
//    "<link rel=\"icon\" href=\"data:,\">"
//    // CSS to style the on/off buttons
//    "<style>"
//        "html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}"
//        "button { background-color: #4CAF50; border: none; color: white; padding: 16px 30px;"
//        "text-decoration: none; font-size: 25px; margin: 2px; cursor: pointer;}"
//        ".btn_off {background-color: #555555;}"
//    "</style>"
//    "</head>"
//    
//    // Web Page Heading
//    "<body><h1>Website Heading</h1>"
//
//    "<p>"
//        "<a href=\"/cmd?on_off\"><button class=\"%ON_OFF_BTN_STATE%\">ON/OFF</button></a>"
//    "</p>"
//
//    "<p>"
//        "<a href=\"/cmd?plus\"><button>PLUS</button></a>"
//        "<a href=\"/cmd?minus\"><button>MINUS</button></a>"
//    "</p>"
//    "</body>"
//    "</html>"
//    "\n";

// Return HTML for API homepage
constexpr const char* api_return_html = index_html;