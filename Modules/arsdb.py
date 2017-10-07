#!/usr/bin/python
# -*- coding: utf-8 -*-

import sys
import pymysql


class ARServerDB:
    def __init__(self, dbname):
        # MySQL Connection
        self.conn = pymysql.connect(host='localhost', user='root', password='abcd1234',
                                    db=dbname, charset='utf8')

    def AddListCheckVM(self, ptype, phash, pname, psize, pdetectb):
        result = 0
        try:
            with self.conn.cursor() as cursor:
                sql = 'INSERT INTO list_check_vm (type, state, hash_hex, file_name, file_size, detected_b, detected_s) VALUES (%s, %s, %s, %s, %s, %s, %s)'
                cursor.execute(sql, (ptype, 'ready', phash, pname, psize, pdetectb, '0'))
            self.conn.commit()
            result = cursor.lastrowid # 1 (last insert id)
        finally:
            if result == 0:
                print('[ERROR] DB - Failed')  # conn.close()
            else:
                print('[INFO] DB - Success(' + str(result) + ')')  # conn.close()
        return result

def main(argv):
    print("Nothing to do. Import me!")
    return 0


if __name__ == '__main__':
    main(sys.argv)

'''
# Connection -> Cursor
curs = conn.cursor()

# Execute SQL
sql = "select * from list_check_vm"
curs.execute(sql)

# Data Fetch
rows = curs.fetchall()
print(rows)     # All rows
# print(rows[0])  # 1 row
# print(rows[1])  # 2 row

# Connection Close
conn.close()
'''