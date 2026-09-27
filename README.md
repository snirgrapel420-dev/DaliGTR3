# DaliGTR — Dali Audio

גיטרה חשמלית וירטואלית היברידית, VST3 ל-Windows ו-macOS. כלי הפקה אישי.
הארכיטקטורה המלאה נמצאת ב-[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md).

## מצב: Phase 1

- מנוע: MIDI → Guitar Brain v0 (מיתר, fret, alternate picking) → 6 מיתרים מונופוניים → הקלטות אמיתיות.
- Sound Set: גיטרת SG (Karoryfer Emilyguitar, CC0) עם 4 שכבות velocity, 3 round robins, רעשי release אמיתיים ותיקון כיוון לכל הקלטה.
- Pitch wheel = bend. שתי יציאות: Main ו-DI נקי.

## הפעלה ראשונה

1. ב-GitHub: **Actions → Build Sound Set → Run workflow**. הריצה יוצרת Release בשם `soundset-v1`. צריך לעשות את זה רק פעם אחת.
2. **Actions → Build DaliGTR** בונה את ה-VST3. מורידים אותו מה-Artifacts.
3. פותחים את DaliGTR ב-Ableton. בפתיחה הראשונה של החלון הפלאגין מוריד את ה-Sound Set לבד, לתיקייה `Documents/Dali Audio/DaliGTR/Sounds`.

אם ה-repo פרטי, GitHub לא מאפשר הורדה ישירה של קבצי Release. במקרה כזה מורידים את `DaliGTR-SoundSet.zip` מדף ה-Release ולוחצים **LOCATE ZIP** בפלאגין.

## טסטים

```
g++ -std=c++17 -O2 -ISource tests/engine_tests.cpp -o engine_tests && ./engine_tests
python tools/soundset/selftest.py
```

## קרדיטים

הקלטות גיטרה: Karoryfer Samples (CC0).
