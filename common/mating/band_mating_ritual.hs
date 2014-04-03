data BandDirective =
  InitMate
  | AckMate

data ServerDirective = SendBandIdentifier -- to Band
  | Mate -- to Band
  | Mated -- to App

data BandMatingRitualInstruction =
  | BandInitMating EncryptedNonce1 {- BandDirective -} -- Band to Server, 4
  | BandAckMating  EncryptedNonce2 {- BandDirective -} BandID -- Band to Server, 6
  | ServerUserLogin Username Password -- Band to Server, 1, REST?
  | ServerUserLoginResponse ServerAppToken -- Server to App, 2, REST?
  | ServerInitiateMate -- Server to App (+ Band?), 3
  | ServerMatingBlob1 EncryptedNonce1 ServerDirective -- Server to Band, 5
  | ServerMatingBlob2 EncryptedNonce2 ServerDirective UserPublicKey -- Server to Band (+ App?), 7
  | Success -- Band to App (+ Server?), 8

{-

implications:

* app initiates a mate
* server controls the entire mating process
* app passes packets until it receives a mated packet from both server and band
- app can peek at packet to see if it's an error or not... if error, needs to decrypt and display error, and mating process stops
-}