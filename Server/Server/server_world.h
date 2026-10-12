#pragma once

int ClampInt(int value, int low, int high);
bool IsBlockedTile(int x, int y);
void RandomOpenPosition(short& x, short& y);
bool SectionFromMapNumber(int number, int& sectionX, int& sectionY);
void RandomOpenPositionInSection(int sectionX, int sectionY, short& x, short& y);