# Matter e Thread
## Cosa sono
Apple insieme ad Amazon, Google, Samsung e tutta la Zigbee Alliance hanno concordato una collaborazione per un nuovo standard sotto il nome di Project CHIP (Connected Home over IP) che è stato poi rinominato in Matter
## Matter
- Matter: set standardizzato di comandi trasmessi su reti IP locali; protocollo che consente la comunicazione fra dispositivi domotici di aziende diverse; usa Thread, Wi‑Fi ed Ethernet come tecnologie di rete sottostanti.
- I dispositivi Matter su Thread utilizzano la rete Mesh (in sostanza è una rete in cui più dispositivi assumono il ruolo di router, cioè di ripetitore. Invece di rispedire semplicemente i segnali alla sorgente di origine, i dispositivi ripetono i segnali e li inoltrano agli altri dispositivi di rete presenti nel loro raggio d’azione)
## Thread
Thread è invece la “novità” tra questi protocolli, perché il suo scopo è di gestire la connessione dei dispositivi della casa connessa a bassa potenza e che non richiedo una banda di trasmissione ampia; thread è una rete mesh, i cui i dispositivi sono cioè in grado di comunicare tra loro e di adattarsi alla variazione della rete, facendo anche fronte ad eventuali guasti.
## Cos'è un gateway
Cos'è un bridge/gateway: I gateway di rete sono progettati per tradurre il traffico tra protocolli diversi. Queste traduzioni consentono di collegare tra loro le reti che utilizzano protocolli o formati di dati diversi, con il gateway di rete che esegue la traduzione in linea. I Gateway Matter sono chiamati "border router", o "outer di confine/frontiera".
## Cos'è Zigbee
Cos'è Zigbee: in concreto, si tratta di un protocollo radio aperto, standard, tale da consentire le trasmissioni dati tra un elemento centrale (un collettore, diciamo) e uno o più componenti domotici (attuatori, dispositivi, sensori ecc.) presenti nel proprio ambiente. Si differenzia dal Wi-Fi (che, concettualmente, ha un ruolo simile) nel fatto di esser concepito principalmente per uso domotico, facendo del basso consumo elettrico e della buona distanza coperta (fino a 100 metri in campo aperto) i punti di forza. La velocità di trasmissione invece è molto bassa
## I prodotti Matter sono compatibili con Zigbee?
- Matter può collegare altre tecnologie, come Zigbee, utilizzando i bridge domestici intelligenti per interagire con i dispositivi che utilizzano quegli altri protocolli. Dal punto di vista tecnico, Matter utilizza parte della stessa tecnologia sottostante a cui sarebbe familiare agli sviluppatori di Zigbee. I dispositivi basati su thread supportano la stessa tecnologia radio 802.15.4 di Zigbee. Matter e Zigbee sono protocolli diversi, offrendo ai produttori diverse opzioni adatte alle loro applicazioni. Ciò significa, tuttavia, che non esiste interoperabilità nativa tra Matter e Zigbee.
- Un Matter bridge consente agli utenti di aggiungere i dispositivi ZigBee e Z-wave esistenti all'ecosistema Matter Fabric (Matter Fabric) e di collaborare con i nuovi dispositivi Matter. Come accennato in precedenza, al momento non è possibile connettere direttamente dispositivi non Matter a Matter. Ciò lascia il bridging come unica opzione praticabile per connettere ZigBee/Z-Wave a Matter.
## Come collegare dispositivi non Matter con dispositivi Matter
Un dispositivo bridge Matter estende la connettività ai dispositivi IoT non Matter in una fabric Matter. Consente al consumatore di continuare a utilizzare i dispositivi non Matter esistenti come i dispositivi Zigbee e Z-Wave insieme ai nuovi dispositivi Matter. Questi dispositivi non-Matter appaiono come dispositivi collegati alla struttura Matter. Il bridge Matter esegue la traduzione del protocollo tra i dispositivi di rete Matter e Zigbee/Z-Wave utilizzando Unify SDK.
## Cos'è Unify SDK
"Unify SDK" di Silicon Labs semplifica lo sviluppo dell'infrastruttura IoT, inclusi gateway, punti di accesso, hub, bridge e prodotti finali basati su processori applicativi. Ogni componente Unify SDK implementa un'interfaccia MQTT per il linguaggio unificato basato su Dotdot. È un'interfaccia modulare, estensibile, leggera e ben definita per l'integrazione del sistema. Unify SDK viene eseguito nativamente su Linux ma è progettato per la portabilità. L'applicazione bridge Unify-Matter, che fa parte di Unify SDK, si basa sul software Matter Bridge Application di CSA (Connectivity Standards Alliance). L'applicazione riceve i comandi ZCL sull'interfaccia del protocollo Matter, li traduce nel modello dati Unify Controller Language e li pubblica su un'interfaccia MQTT.
![immagine](https://github.com/scipioni/beesensor/assets/174588344/7a6b0d3d-b113-45f7-a836-5f98d8d76d34)

(https://connectzbt1.home-assistant.io/about-multiprotocol/#from-skyconnect)

--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
# Home Assistant --> Open Thread Border Router (OTBR)
## What is Home Assistant?
Home Assistant is free and open-source software used for home automation. It serves as an integration platform and smart home hub, allowing users to control smart home devices. The software emphasizes local control and privacy and is designed to be independent of any specific Internet of Things (IoT) ecosystem. Its interface can be accessed through a web-based user interface, by using companion apps for Android and iOS, or by voice commands via a supported virtual assistant, such as Google Assistant, Amazon Alexa, Apple Siri, and Home Assistant's own "Assist" (a built-in local voice assistant) using natural language.
## Thread and Zigbee protocols communication
Thread uses the same RF technology as Zigbee (IEEE 802.15.4) but provides IP connectivity similar to Wi-Fi. Unlike Zigbee, Thread by itself does not allow controlling devices: It is just a communication protocol. To control the Thread devices, a higher-level protocol is required: Matter or Apple HomeKit. Thread devices use the IPv6 standard to communicate both inside and outside the mesh network.
## Open Thread Border Routers
- The devices use Thread border routers to communicate outside the mesh with any IPv6-capable device. A Thread border router is connected to your network either via Wi-Fi or Ethernet and uses its RF radio to communicate with the Thread mesh network. The TBR routes packets between your local network and the Thread mesh. It does not look at the content of these packets, it just forwards them.
- OpenThread is an open source implementation of Thread, originally released by Google. Almost all commercially available Thread border routers are based on the open source implementation. While Home Assistant can use any border router, it can configure and control only OpenThread border routers built with the REST API available in the open source implementation. The OpenThread Border Router add-on (as well as the OpenThread Border Router bundled in the experimental Silicon Labs Multiprotocol add-on) are built from this open source OpenThread code and have the REST API enabled. A REST API (also known as RESTful API) is an application programming interface (API or web API) that conforms to the constraints of REST architectural style and allows for interaction with RESTful web services. (REST = representational state transfer)
- API stands for application programming interface. An API is a set of protocols and instructions written in programming languages such as C++ or JavaScript that determine how two software components will communicate with each other.
## Home Assistant - OTBR communication
- They communicate with each other using Thread protocol. OTBR acts as an intermediary between the thread network and the IP network.
- Home Assistant communicates with OTBR using standard protocols, such as MQTT (Message Queuing Telemetry Transport) or CoAP (Constrained Application Protocol)

https://community.home-assistant.io/t/how-to-configure-preferred-thread-network/542274/18

## Python Matter Server add-on (PMS)
- For communicating with Matter devices, the Home Assistant integration runs its own “Matter controller” as add-on. This Matter Server add-on runs the controller software as a separate process and connects your Matter network (called Fabric in technical terms) and Home Assistant. The Home Assistant Matter integration connects to this server via a WebSocket connection.

Add the Matter (BETA) integration.
When prompted to Select the connection method:

    If you run Home Assistant OS in a regular setup: select Submit.
        This will install the official Matter server add-on.
        Note that the official Matter server add-on is not supported on 32-bit platforms.
    If you are already running the Matter server in another add-on, in or a custom container:
        Deselect the checkbox, then select Submit.
        In the next step, provide the URL to your Matter server.

https://www.home-assistant.io/integrations/matter/



