# GitHub Pages — ESP32 Remote Dashboard

## Τι είναι
Μια στατική σελίδα (GitHub Pages) που συνδέεται στο MQTT (WebSocket) και σου δείχνει:
- BMS δεδομένα
- Relay toggles

Δουλεύει από οπουδήποτε (4G/άλλο WiFi), αρκεί το ESP32 να είναι online και να κάνει publish στο MQTT.

## Πώς το ενεργοποιείς

1. Βεβαιώσου ότι το project είναι σε GitHub repo.
2. Κάνε push τα αρχεία του φακέλου `docs/`.
3. Στο GitHub:
   - Settings → Pages
   - **Source**: Deploy from a branch
   - **Branch**: `main` (ή `master`) / folder **`/docs`**
4. Περίμενε 1–2 λεπτά να βγει URL τύπου:
   - `https://<username>.github.io/<repo>/`

## Χρήση

1. Άνοιξε τη GitHub Pages σελίδα.
2. Βάλε **Device ID** (π.χ. `38182B8BD5CC`) και πάτα **Σύνδεση**.
3. Από εδώ και πέρα το αποθηκεύει (localStorage) και δεν το ξαναζητάει.

## Σημαντικό
Η σελίδα στο GitHub Pages μιλάει **μόνο** με MQTT (όχι με `esp32.local`).  
Άρα:
- Σπίτι: δουλεύει, εφόσον έχει Internet πρόσβαση το κινητό/PC.
- Εκτός: δουλεύει όπως είναι.

