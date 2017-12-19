import com.twitter.common.text.pipeline.TwitterLanguageIdentifier
import com.twitter.pluck.source.StatusSource
import com.twitter.scalding._
import com.twitter.scalding.source.TypedText
import com.twitter.scalding.typed.TypedPipe
import com.twitter.scalding_internal.job.{TwitterJob, RequiredBinaryComparators, TwitterDateJob}
import java.util.Locale
import com.twitter.common.text.pipeline.TwitterLanguageIdentifier
import com.twitter.pluck.source.StatusSource
import com.twitter.pluck.source.combined_user_source.MostRecentCombinedUserSnapshotSource
import com.twitter.scalding._
import com.twitter.scalding.source.TypedText
import com.twitter.scalding.typed.TypedPipe
import com.twitter.scalding_internal.job.{TwitterJob, RequiredBinaryComparators, TwitterDateJob}
import java.util.Locale
import scala.util.Random

/**

Run:
  ./pants compile topics && ./pants bundle topics:topics-deploy
  oscar hdfs --screen \
    --bundle topics-deploy --job AddUserMetadataToUserCounts \
    -- \
    --output usersToQueryCountsWithMetadata

To see the data:
  science/scalding/scripts/hdfsget usersToQueryCountsWithMetadata
  cat usersToQueryCountsWithMetadata
  */
class AddUserMetadataToUserCounts(args : Args) extends TwitterJob(args) with RequiredBinaryComparators {

  val input = TypedText.tsv[UserRow.Tuple]("/user/tstramer/usersToQueryCounts")

  val filteredUsers = TypedPipe.from(MostRecentCombinedUserSnapshotSource)
    // Sanity check on necessary fields.
    .filter { user =>
      user.isSetUser && user.getUser.isSetId && user.getUser.getId > 0L && user.getUser.isSetProfile &&
        user.isSetUser_extended && user.getUser_extended.isSetTweepcred2 && user.getUser_extended.isSetFollowers
    }

  val verifiedUsers = filteredUsers
    .filter { user =>
      (for {
        user <- Option(user.getUser)
        safety <- Option(user.getSafety)
      } yield safety.verified && !safety.is_protected).getOrElse(false)
    }
    .map(user => (user.getUser.getId, user))

  val userIdToCounts = TypedPipe.from(input)
    .map(UserRow.fromTuple)
    .map { row =>
      (row.userID, row.countsJsonStr)
    }

  verifiedUsers
    .group
    .join(userIdToCounts)
    .toTypedPipe
    .map { case (userID, (user, countsJson)) =>
      UserRowDetailed(
        userID = userID,
        tweetCount = Option(user.user.getCounts).map(_.tweets).getOrElse(0),
        followingCount = user.getUser_extended.getFollowings.toLong,
        followerCount = user.getUser_extended.getFollowers.toLong,
        tweepCred = user.getUser_extended.getTweepcred2.toDouble,
        name = Option(user.getUser.getProfile.getName).getOrElse("N/A"),
        screenName = Option(user.getUser.getProfile.getScreen_name).getOrElse("N/A"),
        bio = Option(user.getUser.getProfile.getDescription)
          .getOrElse("N/A")
          .replace("\n", " ").replace("\r", " "),
        countsJsonStr = countsJson
      )
    }
    // write the output to a TypedText.tsv
    .write(TypedText.tsv[UserRowDetailed](args("output")))
}