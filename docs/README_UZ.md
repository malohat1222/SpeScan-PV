# SpeScan-PV — ESP32 prototip firmware v0.2

## Bu versiyada tuzatildi

- **`STOP` darhol qabul qilinadi**: skanerlash `millis()` asosidagi holatli avtomat bilan ishlaydi, 500 ms lik `delay()` bilan bloklanmaydi.
- **Kalibrlanmagan to'lqin uzunliklari bloklangan**: `OPTICAL_CALIBRATION_VALID=false` turadi. Fizik spektral kalibrlashdan keyingina `true` qilinadi.
- **Sensor yo'qolsa nol ma'lumot yozilmaydi**, xato berilib skan to'xtaydi.
- Servo `attach()` tekshiruvi `servo.attached()` bilan almashtirilgan.
- Elektr quvvati **`P_el = U_load * I`** orqali hisoblanadi; bu **optik quvvat** emas.
- Nextion displeyi ixtiyoriy (`USE_NEXTION=false`).
- `SENSOR` va `SERVO:90` dastlabki diagnostika buyruqlari qo'shildi.

## Qaysi imkoniyatlar hali ishlab chiqilmagan?

* Ikki chetli chiqish tirqishining mustaqil aktuatorlari va boshqaruv dasturi.
* Harorat sensorining qurilmasi, ulanishi va o'qilishi.
* Elektron yuklama bilan VAX/MPP o'lchash.
* Etalon optik quvvat detektori orqali SR/EQE kalibrlash.
* Germaniy uchun yaqin infraqizil (>1100 nm) qamrovi.

Hozirgi dastur **bitta servomotorli qaytaruvchi panjara yordamida bitta markaziy to'lqin uzunligini skanerlash konsepsiyasi**ga xizmat qiladi. Bu **keng spektral polosaning ikki chegarasini mustaqil boshqarish** bilan bir xil emas.

## Ulanishlar (ESP32 klassik variant misolida)

| Modul | ESP32 | Izoh |
| --- | --- | --- |
| INA219 SDA | GPIO21 | I2C |
| INA219 SCL | GPIO22 | I2C |
| INA219 VCC / GND | Mos keluvchi modul kuchlanishi / GND | Modul spesifikatsiyasini tekshiring |
| DS3218 signal | GPIO25 | Servo kuchi alohida manbadan! |
| DS3218 GND | ESP32 GND bilan umumiy | Tok quvvat manbaidan olinadi |
| Nextion TX | GPIO32 RX | Majburiy emas |
| Nextion RX | GPIO33 TX | UART kuchlanish darajalarini tekshiring |

**DS3218 quvvatini ESP32 ning 3.3V pinidan bermang.** Servo dastlab optik panjaradan uzilgan holda sinansin. Galogen lampa yuqori harorat va ko'z uchun xavf tug'dirishi mumkin.

## Arduino IDE ishlatish

1. ESP32 board paketini o'rnating va haqiqiy ESP32 modeliga mos platalarni tanlang.
2. Library Manager'dan `Adafruit INA219`, `ESP32Servo` kutubxonalarini o'rnating.
3. `SpeScan_ESP32_Updated.ino` faylini Arduino IDE da oching.
4. Dastlab **optik panjara uzilgan holda** motorni tekshiring.
5. Serial Monitor: **115200 baud**, `Newline` yoki `Both NL & CR`.
6. `HELP`, `STATUS`, `SENSOR`, `SERVO:90` buyruqlarini sinang.
7. `START`, `GOTO:660` buyruqlari hali haqiqiy kalibrlash bo'lmagani uchun qasddan bloklangan. Bu xato emas.

## Spektral kalibrlashdan keyin

1. 400–1100 nm da optik tizim haqiqatan ishlashini va har bir servoburchakka mos haqiqiy markaziy to'lqin uzunligini etalon spektrometr / liniya manbalari bilan tekshiring.
2. `calibration[]` jadvalidagi 5 ta **namunaviy** nuqtani o'lchangan nuqtalarga almashtiring; zarurat bo'lsa nuqtalarni ko'paytiring.
3. Optik og'ishlar va mexanik harakat diapazonini alohida tekshiring.
4. Faqat shundan keyin `OPTICAL_CALIBRATION_VALID=true` qiling.
5. `START`, `STOP`, `GOTO:660` buyruqlari bilan fizik skanerlashni tekshiring.

## CSV haqida

Ustunlar: `wavelength_nm,servo_angle_deg,load_voltage_V,current_A,electric_power_W,shunt_mV,measurement_status`.

`measurement_status=UNVALIDATED_OPTICAL_SETUP` — ushbu ilmiy dastur laboratoriya validatsiyasidan o'tmaganini eslatadi; uni keyinchalik haqiqiy kalibrlash, etalon, o'lchash noaniqligi va sinov protokollari asosida yangilang.

## Xavfsiz yangilash

GitHub'dagi original firmware'ni hozircha o'chirmang. Yangi kodni alohida versiya/fayl sifatida joylashtiring. **Kompilyatsiya va haqiqiy apparatda ishlash hali tekshirilmagan**; mavjud apparat konfiguratsiyasiga moslashtirib sinash talab etiladi.
