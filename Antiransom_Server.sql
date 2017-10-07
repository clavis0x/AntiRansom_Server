CREATE TABLE list_check_vm(
  `idx` int(11) NOT NULL AUTO_INCREMENT,
  `type` char(20) NOT NULL,
  `state` char(20) NOT NULL,
  `hash_hex` char(64) NOT NULL,
  `file_name` varchar(255) NOT NULL,
  `file_size` int(11) NOT NULL,
  `detected_b` int(11) DEFAULT NULL,
  `detected_s` int(11) DEFAULT NULL,
  `date` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`idx`)
) ENGINE=InnoDB AUTO_INCREMENT=1 DEFAULT CHARSET=utf8;