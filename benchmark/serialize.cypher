create node table Person (ID INT64,firstName STRING,lastName STRING,gender STRING,birthday DATE,creationDate TIMESTAMP,locationIP STRING,browserUsed STRING, PRIMARY KEY(ID))
copy Person from "{}/person_0_0.csv" (HEADER=true, DELIM="|")
create node table Forum (ID INT64,title STRING,creationDate TIMESTAMP, PRIMARY KEY(ID))
copy Forum from "{}/forum_0_0.csv" (HEADER=true, DELIM="|")
create node table Post (ID INT64,imageFile STRING,creationDate TIMESTAMP,locationIP STRING,browserUsed STRING,language STRING,content STRING,length INT64, PRIMARY KEY(ID))
copy Post from "{}/post_0_0.csv" (HEADER=true, DELIM="|")
create node table Comment (ID INT64,creationDate TIMESTAMP,locationIP STRING,browserUsed STRING,content STRING,length INT64, PRIMARY KEY(ID))
copy Comment from "{}/comment_0_0.csv" (HEADER=true, DELIM="|")
create node table Tag (ID INT64,name STRING,url STRING, PRIMARY KEY(ID))
copy Tag from "{}/tag_0_0.csv" (HEADER=true, DELIM="|")
create node table Tagclass (ID INT64,name STRING,url STRING, PRIMARY KEY(ID))
copy Tagclass from "{}/tagclass_0_0.csv" (HEADER=true, DELIM="|")
create node table Place (ID INT64,name STRING,url STRING,type STRING, PRIMARY KEY(ID))
copy Place from "{}/place_0_0.csv" (HEADER=true, DELIM="|")
create node table Organisation (ID INT64,type STRING,name STRING,url STRING, PRIMARY KEY(ID))
copy Organisation from "{}/organisation_0_0.csv" (HEADER=true, DELIM="|")
create rel table containerOf (FROM Forum TO Post,ONE_MANY)
copy containerOf from "{}/forum_containerOf_post_0_0.csv" (HEADER=true, DELIM="|")
create rel table comment_hasCreator (FROM Comment TO Person, MANY_ONE)
copy comment_hasCreator from "{}/comment_hasCreator_person_0_0.csv" (HEADER=true, DELIM="|")
create rel table post_hasCreator (FROM Post TO Person,MANY_ONE)
copy post_hasCreator from "{}/post_hasCreator_person_0_0.csv" (HEADER=true, DELIM="|")
create rel table hasInterest (FROM Person TO Tag, MANY_MANY)
copy hasInterest from "{}/person_hasInterest_tag_0_0.csv" (HEADER=true, DELIM="|")
create rel table hasMember (FROM Forum TO Person,joinDate TIMESTAMP,MANY_MANY)
copy hasMember from "{}/forum_hasMember_person_0_0.csv" (HEADER=true, DELIM="|")
create rel table hasModerator (FROM Forum TO Person,MANY_ONE)
copy hasModerator from "{}/forum_hasModerator_person_0_0.csv" (HEADER=true, DELIM="|")
create rel table comment_hasTag (FROM Comment TO Tag,MANY_MANY)
copy comment_hasTag from "{}/comment_hasTag_tag_0_0.csv" (HEADER=true, DELIM="|")
create rel table forum_hasTag (FROM Forum TO Tag,MANY_MANY)
copy forum_hasTag from "{}/forum_hasTag_tag_0_0.csv" (HEADER=true, DELIM="|")
create rel table post_hasTag (FROM Post TO Tag,MANY_MANY)
copy post_hasTag from "{}/post_hasTag_tag_0_0.csv" (HEADER=true, DELIM="|")
create rel table hasType (FROM Tag TO Tagclass,MANY_ONE)
copy hasType from "{}/tag_hasType_tagclass_0_0.csv" (HEADER=true, DELIM="|")
create rel table comment_isLocatedIn (FROM Comment TO Place,MANY_ONE)
copy comment_isLocatedIn from "{}/comment_isLocatedIn_place_0_0.csv" (HEADER=true, DELIM="|")
create rel table organisation_isLocatedIn (FROM Organisation TO Place,MANY_ONE)
copy organisation_isLocatedIn from "{}/organisation_isLocatedIn_place_0_0.csv" (HEADER=true, DELIM="|")
create rel table person_isLocatedIn (FROM Person TO Place,MANY_ONE)
copy person_isLocatedIn from "{}/person_isLocatedIn_place_0_0.csv" (HEADER=true, DELIM="|")
create rel table post_isLocatedIn (FROM Post TO Place,MANY_ONE)
copy post_isLocatedIn from "{}/post_isLocatedIn_place_0_0.csv" (HEADER=true, DELIM="|")
create rel table isPartOf (FROM Place TO Place,MANY_ONE)
copy isPartOf from "{}/place_isPartOf_place_0_0.csv" (HEADER=true, DELIM="|")
create rel table isSubclassOf (FROM Tagclass TO Tagclass,MANY_ONE)
copy isSubclassOf from "{}/tagclass_isSubclassOf_tagclass_0_0.csv" (HEADER=true, DELIM="|")
create rel table knows (FROM Person TO Person,creationDate TIMESTAMP,MANY_MANY)
copy knows from "{}/person_knows_person_0_0.csv" (HEADER=true, DELIM="|")
create rel table likes_comment (FROM Person TO Comment,creationDate TIMESTAMP,MANY_MANY)
copy likes_comment from "{}/person_likes_comment_0_0.csv" (HEADER=true, DELIM="|")
create rel table likes_post (FROM Person TO Post,creationDate TIMESTAMP,MANY_MANY)
copy likes_post from "{}/person_likes_post_0_0.csv" (HEADER=true, DELIM="|")
create rel table replyOf_comment (FROM Comment TO Comment,MANY_ONE)
copy replyOf_comment from "{}/comment_replyOf_comment_0_0.csv" (HEADER=true, DELIM="|")
create rel table replyOf_post (FROM Comment TO Post,MANY_ONE)
copy replyOf_post from "{}/comment_replyOf_post_0_0.csv" (HEADER=true, DELIM="|")
create rel table studyAt (FROM Person TO Organisation,classYear INT64,MANY_MANY)
copy studyAt from "{}/person_studyAt_organisation_0_0.csv" (HEADER=true, DELIM="|")
create rel table workAt (FROM Person TO Organisation,workFrom INT64,MANY_MANY)
copy workAt from "{}/person_workAt_organisation_0_0.csv" (HEADER=true, DELIM="|")