# Synthapsis

## Introduzione

Synthapsis e' un sintetizzatore portatile progettato con l'intento di estrarre melodie dal chaos.
Genera note casuali seguendo un ordine stocastico definito da una matrice di Markov, modificabile in
tempo reale mediante connessioni fisiche tra pin.

## Funzionalita'

Il dispositivo è costituito da un'interfaccia a 24 nodi metallici (12 input e 12 output) tramite
i quali l'utente, utilizzando cavi con clip a coccodrillo, modella dinamicamente una catena di Markov.
Questa struttura definisce le probabilità di transizione tra le dodici note della scala cromatica:
la probabilità di passare dalla nota corrente a una delle note successive è distribuita uniformemente
tra tutti i nodi di destinazione fisicamente collegati al pin di partenza.

Per la selezione della nota successiva, al fine di evitare i pattern prevedibili tipici dei generatori
di numeri pseudo-casuali (PRNG), il software impiega un True Random Number Generator (TRNG) che sfrutta
l'hardware dell'ESP32 per estrarre entropia basata sul rumore di clock.

Dei LED disposti sotto ciascuno pin supportano la user-experience, indicando in tempo reale la nota
attualmente riprodotta mediante la loro accensione.

Le idee sulla disposizione dei pin, e conseguentemente dei LED, e' ancora da definirsi bene. Tuttavia
quella che a mio avviso sembra la piu' compatta e comoda da utilizzare e' quella di creare 2 cerchi
concentrici: su quello esterno si dispongono i pin di uscita, mentre su quello interno i pin di ingresso;
i LED sono disposti a meta' tra i due cerchi.
