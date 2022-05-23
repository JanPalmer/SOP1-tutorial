#!/bin/bash

for n in {1..11};
do
    ./client localhost 2000 10 &
done