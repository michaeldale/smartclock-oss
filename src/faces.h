#pragma once
#include <time.h>

// Clock faces. Ids match the stock theme numbers and /api/set?key=theme.
enum Face { FACE_CLASSIC = 0, FACE_WEATHER = 1, FACE_PHOTO = 2, FACE_DIAL = 3,
            FACE_SIMPLE = 4, FACE_FORECAST = 5, FACE_FLIP = 6, FACE_COUNT = 7,
            FACE_CUSTOM = 7 };   // cfg.customFace, from /faces/<name>.json (integrations)

// Draws only what changed since the last call; a face switch or faceInvalidate()
// redraws everything. timeValid is false until NTP has set the clock.
void faceDraw(int face, const struct tm& t, bool timeValid);
void faceInvalidate();            // something else drew on the panel, or settings changed
bool faceAvailable(int face);     // has something to show (weather, forecast, photos)
void facePhotosChanged();         // a photo was added or removed
int  photoCount();
