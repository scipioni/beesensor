```mermaid
  graph LR;
1(S3) -.- 3(S1);
2(S2) -.- 3(S1);
1(S3) -.- 2(S2);
3(S1) -.- 4(GW);
4(GW) -.- 5(Z2M)
5(Z2M) --- 6(MQTT);
6(MQTT) --- 7(Client);
```
