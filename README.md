# Proyecto 1 de Sistemas Operativos - Instituto Tecnológico de Costa Rica
## Shared Memory

### Commands index

##### Compile files.

How to compile C files.

```
gcc initializer.c -o init -pthread
gcc writer.c -o writer -pthread -lm
gcc reader.c -o reader -pthread -lm
```

##### Using initializer.

Create a buffer.
```
./init create <name> <size>
./init create MyBuffer 1024
```

Delete a buffer.
```
./init destroy <ID>
./init destroy 3600
```

##### Using writer.

Create a message.
```
./writer <ID> <wait mean> <message>
./writer 3600 0.4 "Hello There!"
```

##### Using reader.

Read a message.
```
./reader <ID> <wait mean> <operation mode>
./reader 3600 4 auto
```


