#pragma once
// Web server: the panel at "/", the JSON API under /api/v1 and the live state over the WebSocket
// /api/v1/ws (signed in). POST requests take form fields; answers are JSON, errors {"error":"<code>"}.

namespace web {

void begin();
void loop();  // loop(), without StateLock: pushes live state, restarts after OTA

}  // namespace web
