# Spectre v2 (BTB Injection) — Proof of Concept

Proof of Concept didattica che dimostra l'attacco **Spectre v2 (Branch Target Injection)**, sfruttando l'esecuzione speculativa e un canale laterale basato sulla cache (Flush+Reload) per estrarre dati sensibili dalla memoria di processo.

Realizzata nell'ambito di attività di ricerca su vulnerabilità microarchitetturali.

## Contesto

Le CPU moderne utilizzano predittori di branch per anticipare il target dei salti indiretti, eseguendo speculativamente le istruzioni successive prima di conoscere il target reale. Se la predizione è errata, le istruzioni eseguite speculativamente vengono annullate — ma i loro effetti collaterali sulla cache **non vengono ripristinati**. Questo PoC dimostra come tali effetti possano essere osservati per ricostruire dati che non dovrebbero essere accessibili.

## Come funziona

Il flusso dell'attacco si articola in cinque fasi, ripetute per ogni byte del segreto:

1. **Training del Branch Target Buffer (BTB)**
   Un puntatore a funzione viene invocato ripetutamente puntando a `funzione_vittima`, per addestrare il predittore di branch ad associare quel sito di chiamata a quel target.

2. **Flush del sensor array**
   L'intero *sensor array* (256 pagine da `STRIDE` byte) viene evitato dalla cache tramite `_mm_clflush`, così da partire da uno stato noto per la fase di reload.

3. **Flush della catena di puntatori**
   Anche i nodi della catena (`n1`, `n2`, `n3`) vengono rimossi dalla cache, in modo che il loro caricamento nella fase successiva richieda tempo e lasci margine all'esecuzione speculativa.

4. **Attacco speculativo (misprediction)**
   La CPU attraversa la catena di puntatori; poiché i nodi non sono in cache, il processore specula sul branch predetto in fase di training e **esegue speculativamente** `funzione_vittima` con l'indice del byte segreto come argomento, prima ancora di aver risolto il puntatore reale. Durante questa finestra speculativa, il valore segreto viene usato per indicizzare `sensor_array`, portando in cache la pagina corrispondente al suo valore.

5. **Reload (canale laterale)**
   Per ciascuno dei 256 possibili valori di byte, si misura il tempo di accesso alla pagina corrispondente in `sensor_array` tramite `__rdtscp`. Un tempo sotto la soglia calibrata indica un *cache hit*, ovvero che quella pagina è stata toccata durante l'esecuzione speculativa — rivelando il valore del byte segreto.

Il processo viene ripetuto per `ATTACK_REPS` iterazioni per ogni posizione, accumulando uno *score* per ciascun valore candidato; il valore con lo score più alto viene considerato quello corretto.

## Componenti principali

| Elemento | Ruolo |
|---|---|
| `sensor_array` | Array usato come canale laterale basato su cache (un blocco da `STRIDE` byte per ogni valore possibile 0–255) |
| `funzione_vittima` | Simula il codice che accede a un dato segreto in modo condizionato, target dell'esecuzione speculativa |
| `funzione_training` | Target "legittimo" usato per addestrare il predittore di branch |
| Catena di puntatori (`n1`, `n2`, `n3`) | Introduce latenza controllata per aprire la finestra speculativa al momento dell'attacco |
| Calibrazione soglia | Misura i tempi di cache hit/miss di riferimento per distinguere gli accessi speculativi da rumore |

## Compilazione ed esecuzione

```bash
gcc -O0 -march=native poc_spectre_v2.c -o poc_spectre_v2
./poc_spectre_v2
```

**Requisiti**: CPU x86 con supporto a `RDTSCP` e `CLFLUSH` (`x86intrin.h`).

**Nota**: efficacia e affidabilità dell'attacco dipendono fortemente da microarchitettura, mitigazioni del kernel/firmware (es. IBRS, retpoline) e rumore di sistema. Su CPU con mitigazioni Spectre attive l'attacco potrebbe non produrre risultati affidabili.

## Output atteso

Per ogni posizione del segreto, il programma stampa il byte con lo score più alto:

```
Calibrazione: HIT <ciclì>, MISS <cicli>, SOGLIA <valore>
Pos 0: 's' (Score: ...)
Pos 1: 'u' (Score: ...)
...
```

## Finalità e limitazioni

Questo codice è stato sviluppato **esclusivamente a scopo didattico e di ricerca**, per analizzare e documentare la classe di vulnerabilità Spectre v2 nell'ambito di un percorso accademico in sicurezza informatica. Non è pensato per l'uso contro sistemi di terzi senza autorizzazione esplicita.

## Riferimenti

- Kocher et al., *Spectre Attacks: Exploiting Speculative Execution*, 2019
- Documentazione Intel su mitigazioni Spectre (IBRS/IBPB, retpoline)
