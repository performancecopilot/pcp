[![Build Status](https://travis-ci.com/Erbenos/pcp-statsd-c.svg?branch=aggregating-values)](https://travis-ci.com/Erbenos/pcp-statsd-c)

# pcp-statsd-c
PCP PMDA for StatsD in C

You need to have **chan** [repo](https://github.com/tylertreat/chan) and **HdrHistogram_c** [repo](https://github.com/HdrHistogram/HdrHistogram_c) installed in your /usr/local dir. You also need to have **Ragel** installed. 

## Running pcp-statsd-c
Compile with:

```
make
```

Run with:

You might need right priviledges to do this. 

```
make run
```

Run tests with:

```
make test-basic
make test-ragel
```

Clean with:

```
make clean
```

### These commands require right priviledges, they have no other dependencies in Makefile other than themselves (so they expect you already ran Make to produce some binary builds).

Debug local binary with dbpmda:
```
sudo make debug
```

Move to PCP_PMDAS_DIRECTORY with:

```
sudo make install
```

First make sure you have "STATSD" namespace set to "510" in stdpmid file. [How-to](https://pcp.io/books/PCP_PG/html/id5189538.html)

Run PMDA with:

```
sudo make activate
```

Remove from PCP_PMDAS_DIRECTORY with:

```
sudo make uninstall
```

Stop PMDA with:

```
sudo make deactivate
```

## FAQ/Troubleshooting

### I installed both **chan** and **HdrHistogram_c** yet the program won't run... there seem to be .so missing.
You may need to make sure that /usr/local is actually looked into. You may need to add the directory to **/etc/ld.so.conf** yourself:
```
tee /etc/ld.so.conf.d/local.conf <<EOF
/usr/local/lib
/usr/local/lib64
EOF
```
Next, run as root:
```
ldconfig
```
to clear linker cache.
