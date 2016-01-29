--cdb = require("cdb")
--rex = require("rex_pcre")

function get_backend_from_username(username, db)
   return db..".sql.example.net", "3306"
end
