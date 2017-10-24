#!/bin/sh
# 
# Recipe for creating the mysql archive
#

. $PCP_DIR/etc/pcp.env

here=`pwd`
tmp=/tmp/$$
rm -rf $tmp

if pmprobe mysql 2>&1 | grep -q 'Unknown metric name'
then
    echo "Arrg, mysql PMDA is apparently not installed"
    exit 1
fi

version=0
while [ -f mysql-$version.0 ]
do
    version=`expr $version + 1`
done

trap "rm -rf $tmp; exit" 0 1 2 3 15

echo 'log mandatory on 5sec { mysql }' >$tmp.config
${PCP_BINADM_DIR}/pmlogger -s 5 -c $tmp.config mysql-$version &

# Now do some work to make mysql stats move a little
# We assume the example classicmodels DB has been installed
# (available from http://www.mysqltutorial.org/mysql-sample-database.aspx)
# and is available to be used.
#
# May need to change the user and password ...
SQLUSER=root
SQLPASSWORD=letmein

for i in 1 2 3 4 5 6
do
    sleep 4
    cat <<End-of-File | mysql -u$SQLUSER -p$SQLPASSWORD classicmodels
select count(*) from customers;
select count(*) from payments;
select count(*) from employees;
select count(*) from offices;
select count(*) from orders;
select count(*) from orderdetails;
select count(*) from products;
select count(*) from productlines;

select productCode, productName, textDescription
from products c1 inner join productlines c2
on c1.productline = c2.productline;

select c1.orderNumber, status, sum(quantityOrdered*priceEach) total
from orders as c1 inner join orderdetails
as c2 on c1.orderNumber = c2.orderNumber
group by orderNumber;

End-of-File
done

wait
