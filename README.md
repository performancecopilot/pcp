[![Build Status](https://travis-ci.com/Erbenos/pcp-statsd-c.svg?branch=aggregating-values)](https://travis-ci.com/Erbenos/pcp-statsd-c)

# pcp-statsd-c
PCP PMDA for StatsD in C

You need to have **chan** [repo](https://github.com/tylertreat/chan) and **HdrHistogram_c** [repo](https://github.com/HdrHistogram/HdrHistogram_c) installed in your /usr/local dir. You also need to have **Ragel** installed. 

## Installing **chan**
```
./autogen.sh
./configure
sudo make install
```

## Installing **HdrHistogram_c**
```
cmake .
sudo make install
```

# Installing **Ragel** (Fedora 30)
```
dnf install ragel
```

## Running pcp-statsd-c
compile with:

```
make
```

run with: 

```
make run
```

run tests with:

```
make test
```

clean with:

```
make clean
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
