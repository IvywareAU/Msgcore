// Copyright © 2026 Khrustal & Mann
//              MELBOURNE, VICTORIA, AUSTRALIA, 3000
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
// implied. See the License for the specific language governing
// permissions and limitations under the License.
//
//  Platform layer — crypto backend selector for P2PCngCrypto.
//
//  Part of the Msgcore + Targetcore Linux port (see the Linux port plan §6.2, Risk #5).
//
//  P2PCngCrypto keeps its header/namespace (p2pcng, void* handles). Backends:
//    Windows -> cng.cpp     (BCrypt: AES-256-GCM, ECDH P-256, HKDF/HMAC-SHA256, RNG)
//    Linux   -> openssl.cpp (OpenSSL 3: RAND_bytes, EVP AES-256-GCM, EVP ECDH P-256,
//                            EVP_KDF/EVP_MAC; ConstTimeEqual -> CRYPTO_memcmp)
//  The ECDH public-key blob is pinned to the raw uncompressed point so a CNG peer and an
//  OpenSSL peer agree on the wire; cross-backend known-answer tests gate Phase 4.
//
//  Phase status: this header only selects the backend; the live crypto stays in
//  P2PCngCrypto.h/.cpp. The OpenSSL backend is a Phase-4 deliverable.
//
#pragma once

#if defined(_WIN32)
  #define P2P_CRYPTO_BACKEND_CNG     1
  #define P2P_CRYPTO_BACKEND_OPENSSL 0
#else
  #define P2P_CRYPTO_BACKEND_CNG     0
  #define P2P_CRYPTO_BACKEND_OPENSSL 1   // OpenSSL 3 (P-256 matches CNG exactly; §Risk 5)
#endif
