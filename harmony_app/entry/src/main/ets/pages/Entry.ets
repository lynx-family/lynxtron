@Entry
@Component
struct EntryPage {
  build() {
    // EntryAbility does not host the actual application window.
    // It only launches the first WindowAbility. Keep this page empty.
    Stack() {
      Text('Hello Lynxtron')
        .fontSize(46)
    }
    .width('100%')
    .height('100%')
    .backgroundColor('#00000000')
  }
}
