#!/bin/bash
# Copia el contrato comun y secrets a cada nodo (Wokwi no comparte includes)
cd "$(dirname "$0")/.."
[ -f common/secrets.h ] || cp common/secrets.example.h common/secrets.h
for d in nodos/*/; do cp common/g2_common.h common/secrets.h "$d"; done
