import com.twitter.roastduck.util.JsonSerializer

final case class UserRow(
  userID: Long,
  countsJsonStr: String
) {
  val counts = JsonSerializer.invert[Map[String, Int]](countsJsonStr).get
}

object UserRow {
  type Tuple = (Long, String)

  def fromTuple(tuple: Tuple): UserRow = {
    UserRow(tuple._1, tuple._2)
  }
}

final case class UserRowDetailed(
  userID: Long,
  tweetCount: Long,
  followingCount: Long,
  followerCount: Long,
  tweepCred: Double,
  name: String,
  screenName: String,
  bio: String,
  countsJsonStr: String
) {
  val counts = JsonSerializer.invert[Map[String, Int]](countsJsonStr).get
}

object UserRowDetailed {
  type Tuple = (Long, Long, Long, Long, Double, String, String, String, String)

  def fromTuple(tuple: Tuple): UserRowDetailed = {
    UserRowDetailed(tuple._1, tuple._2, tuple._3, tuple._4, tuple._5, tuple._6, tuple._7, tuple._8, tuple._9)
  }
}